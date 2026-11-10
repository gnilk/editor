//
// Created by gnilk on 04.06.26.
//

#include <algorithm>
#include <cmath>
#include "TerminalScreen.h"
#include "Core/Config/Config.h"

using namespace gedit;

TextBuffer::Ref TerminalScreen::MakeScrollbackBuffer() {
    // Plain line store: no language attached (no async tokenizer running over scrollback - it is
    // mutated only from RowToLine under screenLock), read-only (it is a history, not an editable
    // buffer).
    auto buffer = std::make_shared<TextBuffer>();
    buffer->SetReadOnly(true);
    return buffer;
}

TerminalScreen::TerminalScreen() : palette(DefaultPalette()) {
    // A working size until the view's first Resize: output that arrives before it (the shell banner) is kept and
    // reflowed into place - top rows go to scrollback - instead of dropped. Cols()/Rows() report 0 until then.
    terminal = std::make_unique<gnilk::vt::Terminal>(static_cast<gnilk::vt::ITerminalHost &>(*this), 80, 24);
    scrollback = MakeScrollbackBuffer();
    // Every scrollback line must belong to some block (§7) so eviction can always "drop the oldest
    // whole block" - open the implicit loose block up front to cover whatever lands before the first
    // real BeginCommandBlock (startup banner, output with no committed command).
    CommandBlock loose;
    loose.id          = nextBlockId++;
    loose.startAbsRow = AbsRowCount();
    loose.source      = CommandBlock::Source::kHeuristic;
    blocks.push_back(std::move(loose));
}

void TerminalScreen::BeginCommandBlock(const std::u32string &command, CommandBlock::Source src) {
    if (IsAltScreen()) {
        return;   // §10: no block opens/closes while an alt-screen app owns the grid.
    }
    uint64_t pos = AbsRowCount();
    // Closes the previous open block (it is always the tail - at most one open block at a time).
    if (!blocks.empty() && !blocks.back().endAbsRow.has_value()) {
        blocks.back().endAbsRow = pos;
    }
    CommandBlock block;
    block.id          = nextBlockId++;
    block.command     = command;
    block.startAbsRow = pos;
    block.source      = src;
    blocks.push_back(std::move(block));
}

void TerminalScreen::EndOpenBlock(std::optional<int> exitCode) {
    if (IsAltScreen()) {
        return;   // §10
    }
    if (blocks.empty() || blocks.back().endAbsRow.has_value()) {
        return;
    }
    blocks.back().endAbsRow = AbsRowCount();
    blocks.back().exitCode  = exitCode;
}

void TerminalScreen::SetOpenBlockExit(int code) {
    if (!IsAltScreen() && !blocks.empty() && !blocks.back().endAbsRow.has_value()) {
        blocks.back().exitCode = code;
    }
}

void TerminalScreen::SetOpenBlockCwd(const std::filesystem::path &cwd) {
    if (!IsAltScreen() && !blocks.empty() && !blocks.back().endAbsRow.has_value()) {
        blocks.back().cwd = cwd;
    }
}

std::vector<std::u32string> TerminalScreen::GetBlockOutputText(uint64_t blockId) const {
    std::vector<std::u32string> out;

    const CommandBlock *block = nullptr;
    for (const auto &candidate : blocks) {
        if (candidate.id == blockId) {
            block = &candidate;
            break;
        }
    }
    if (block == nullptr) {
        return out;
    }

    uint64_t start = block->startAbsRow;
    uint64_t end   = block->endAbsRow.value_or(AbsRowCount());   // open block runs to the live row
    for (uint64_t abs = start; abs < end; abs++) {
        AbsRow located = RowAtAbs(abs);
        if (auto *line = std::get_if<Line::Ref>(&located)) {
            out.push_back((*line)->Buffer());
        } else if (auto *gridRow = std::get_if<const Row *>(&located)) {
            // Convert a live grid row through RowToLine so it trims identically to how it will look
            // once it scrolls into scrollback - the open block's text is stable across that transition.
            out.push_back(RowToLine(**gridRow)->Buffer());
        }
        // std::monostate -> evicted / out of range; skip (a retained block is complete, §7).
    }
    return out;
}

TerminalScreen::ScrollbackSnapshot TerminalScreen::SnapshotScrollbackTail(size_t maxLines) const {
    ScrollbackSnapshot snap;
    if (IsAltScreen()) {
        return snap;   // §10: alt-screen history is never persisted
    }
    uint64_t numLines = scrollback->NumLines();
    if (numLines == 0) {
        return snap;   // only a live grid so far - the grid is never persisted (§8.1)
    }

    // Keep the last `maxLines` scrollback lines (maxLines == 0 -> keep all).
    uint64_t keep = numLines;
    if (maxLines != 0 && keep > (uint64_t)maxLines) {
        keep = (uint64_t)maxLines;
    }
    uint64_t firstKeptRel    = numLines - keep;                 // index of the first kept scrollback line
    uint64_t firstKeptAbs    = scrollbackBase + firstKeptRel;
    uint64_t scrollbackEnd   = scrollbackBase + numLines;       // exclusive; boundary to the live grid

    snap.lines.reserve((size_t)keep);
    for (uint64_t i = firstKeptRel; i < numLines; i++) {
        snap.lines.push_back(scrollback->LineAt((size_t)i));
    }

    for (const auto &b : blocks) {
        uint64_t bStart = b.startAbsRow;
        uint64_t bEnd   = b.endAbsRow.value_or(AbsRowCount());   // open block runs to the live row
        // Clip the block to the persisted slice [firstKeptAbs, scrollbackEnd).
        uint64_t cStart = std::max(bStart, firstKeptAbs);
        uint64_t cEnd   = std::min(bEnd, scrollbackEnd);
        if (cStart >= cEnd) {
            continue;   // entirely outside the slice (evicted-below, empty, or live-grid-only)
        }
        PersistedBlock pb;
        pb.id        = b.id;
        pb.command   = b.command;
        pb.startLine = cStart - firstKeptAbs;
        pb.endLine   = cEnd   - firstKeptAbs;
        // The exit code survives only for a block whose real end is INSIDE the saved slice. A block
        // clipped at the scrollback boundary (its tail was still in the live grid - includes the open
        // block, closed-at-tail) is persisted as "unknown".
        if (b.endAbsRow.has_value() && (b.endAbsRow.value() <= scrollbackEnd)) {
            pb.exitCode = b.exitCode;
        }
        pb.cwd    = b.cwd;
        pb.source = b.source;
        snap.blocks.push_back(std::move(pb));
    }
    return snap;
}

void TerminalScreen::SeedScrollback(const std::vector<Line::Ref> &lines, const std::vector<PersistedBlock> &seedBlocks) {
    if (lines.empty()) {
        return;   // nothing persisted - keep the ctor's fresh loose block
    }

    // Replace the history wholesale: a fresh read-only buffer holding exactly the seeded lines, based at
    // abs 0 (the seeded lines occupy abs ids [0, lines.size())).
    scrollback = MakeScrollbackBuffer();
    for (const auto &line : lines) {
        scrollback->AddLine(line);
    }
    scrollbackBase = 0;

    // Rebuild the block index: every seeded block is CLOSED (its range == the persisted line indices),
    // then a fresh OPEN loose block covers everything from the seed boundary onward so post-restore
    // output still belongs to a block (§7).
    blocks.clear();
    uint64_t maxId     = 0;
    uint64_t lineCount = lines.size();
    for (const auto &pb : seedBlocks) {
        CommandBlock block;
        block.id          = pb.id;
        block.command     = pb.command;
        block.startAbsRow = std::min<uint64_t>(pb.startLine, lineCount);
        block.endAbsRow   = std::min<uint64_t>(pb.endLine, lineCount);
        block.exitCode    = pb.exitCode;
        block.cwd         = pb.cwd;
        block.source      = pb.source;
        blocks.push_back(std::move(block));
        if (pb.id > maxId) {
            maxId = pb.id;
        }
    }
    nextBlockId = maxId + 1;

    CommandBlock loose;
    loose.id          = nextBlockId++;
    // When the seed carried no blocks (corrupt/legacy), the loose block must still cover the seeded
    // lines so none are orphaned; otherwise it starts at the seed boundary (== AbsRowCount() while the
    // grid is empty at restore time).
    loose.startAbsRow = seedBlocks.empty() ? 0 : AbsRowCount();
    loose.source      = CommandBlock::Source::kHeuristic;
    blocks.push_back(std::move(loose));
}

void TerminalScreen::TrimScrollbackIfNeeded() {
    int cap = Config::Instance()["terminal"].GetInt("scrollback_lines", 10000);
    if (cap <= 0) {
        return;   // 0 = unlimited
    }
    // Block-granular (§7): drop the oldest WHOLE block so a retained block is always complete. Never
    // evict the open (tail) block - `blocks.size() > 1` guarantees `front()` isn't it.
    while ((int)scrollback->NumLines() > cap && blocks.size() > 1) {
        auto &front = blocks.front();
        uint64_t blockLines = front.endAbsRow.value() - front.startAbsRow;
        for (uint64_t i = 0; i < blockLines; i++) {
            scrollback->DeleteLineAt(0);
        }
        scrollbackBase += blockLines;
        blocks.pop_front();
    }
}

Line::Ref TerminalScreen::RowToLine(const Row &row) const {
    // A wide character's right half is not text
    Row cells;
    cells.reserve(row.size());
    for (const auto &cell : row) {
        if (cell.width != 0) {
            cells.push_back(cell);
        }
    }

    std::u32string text;
    text.reserve(cells.size());
    for (const auto &cell : cells) {
        text.push_back(cell.ch);
    }

    // Line's ctor rtrims trailing whitespace - matches every other Line in the editor and drops the
    // blank padding cells without losing any visible content.
    auto line = Line::Create(text);
    auto trimmedLen = (int)line->Length();

    // Run-length encode maximal same fg/bg/attrs spans into LineAttrib spans - the same batching
    // DrawScreenRow does for live rendering, so the converted Line reproduces the original ANSI colors.
    int x = 0;
    while (x < trimmedLen) {
        const Cell &runCell = cells[x];
        Line::LineAttrib attrib;
        attrib.idxOrigString   = x;
        attrib.foregroundColor = runCell.fg;
        attrib.backgroundColor = runCell.bg;
        attrib.textAttributes  = kTextAttributes::kNormal;
        if (runCell.attrs & kAttrBold)      { attrib.textAttributes = attrib.textAttributes | kTextAttributes::kBold; }
        if (runCell.attrs & kAttrItalic)    { attrib.textAttributes = attrib.textAttributes | kTextAttributes::kItalic; }
        if (runCell.attrs & kAttrUnderline) { attrib.textAttributes = attrib.textAttributes | kTextAttributes::kUnderline; }
        if (runCell.attrs & kAttrStrike)    { attrib.textAttributes = attrib.textAttributes | kTextAttributes::kStrikethrough; }
        line->Attributes().push_back(attrib);

        int y = x + 1;
        while (y < trimmedLen
               && cells[y].fg == runCell.fg
               && cells[y].bg == runCell.bg
               && cells[y].attrs == runCell.attrs) {
            y++;
        }
        x = y;
    }
    return line;
}

void TerminalScreen::Feed(const uint8_t *data, size_t length) {
    isRowCacheStale = true;
    terminal->Feed(data, length);
    isRowCacheStale = true;
}

void TerminalScreen::Feed(std::string_view data) {
    Feed(reinterpret_cast<const uint8_t *>(data.data()), data.size());
}

void TerminalScreen::Resize(int newCols, int newRows) {
    isRowCacheStale = true;
    if ((newCols <= 0) || (newRows <= 0)) {
        isSized = false;
        return;
    }
    // Rows above the cursor that no longer fit go to scrollback (OnRowScrolledOut) - content stays anchored to
    // the cursor, the rows below it are padding.
    terminal->Resize(newCols, newRows);
    isSized = true;
}

void TerminalScreen::SetDefaultColors(ColorRGBA fg, ColorRGBA bg) {
    defaultFg = fg;
    defaultBg = bg;
    isRowCacheStale = true;
}

void TerminalScreen::SetPalette(const std::array<ColorRGBA, 256> &colors) {
    palette = colors;
    isRowCacheStale = true;
}

// xterm's 256 colours: 16 system colours (kitty's), the 6x6x6 cube, 24 greys
std::array<ColorRGBA, 256> TerminalScreen::DefaultPalette() {
    static const uint8_t kSystem[16][3] = {
        {0x00, 0x00, 0x00}, {0xcd, 0x00, 0x00}, {0x00, 0xcd, 0x00}, {0xcd, 0xcd, 0x00},
        {0x00, 0x00, 0xee}, {0xcd, 0x00, 0xcd}, {0x00, 0xcd, 0xcd}, {0xe5, 0xe5, 0xe5},
        {0x7f, 0x7f, 0x7f}, {0xff, 0x00, 0x00}, {0x00, 0xff, 0x00}, {0xff, 0xff, 0x00},
        {0x5c, 0x5c, 0xff}, {0xff, 0x00, 0xff}, {0x00, 0xff, 0xff}, {0xff, 0xff, 0xff},
    };
    static const int kCube[6] = {0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff};
    std::array<ColorRGBA, 256> colors;
    for (int i = 0; i < 16; i++) {
        colors[i] = ColorRGBA::FromRGB((int)kSystem[i][0], (int)kSystem[i][1], (int)kSystem[i][2]);
    }
    for (int i = 0; i < 216; i++) {
        colors[16 + i] = ColorRGBA::FromRGB(kCube[(i / 36) % 6], kCube[(i / 6) % 6], kCube[i % 6]);
    }
    for (int i = 0; i < 24; i++) {
        int v = 8 + i * 10;
        colors[232 + i] = ColorRGBA::FromRGB(v, v, v);
    }
    return colors;
}

const TerminalScreen::Row &TerminalScreen::GetRow(int y) const {
    static const Row empty;
    if ((y < 0) || (y >= Rows())) {
        return empty;
    }
    RefreshRowCache();
    return rowCache[y];
}

TextBuffer::Ref TerminalScreen::GetScrollback() const {
    return scrollback;
}

uint64_t TerminalScreen::AbsRowCount() const {
    return scrollbackBase + scrollback->NumLines() + GetCursorPos().y;
}

TerminalScreen::AbsRow TerminalScreen::RowAtAbs(uint64_t abs) const {
    if (abs < scrollbackBase) {
        return std::monostate{};   // evicted
    }
    uint64_t rel = abs - scrollbackBase;
    uint64_t scrollbackLines = scrollback->NumLines();
    if (rel < scrollbackLines) {
        return scrollback->LineAt((size_t)rel);
    }
    int gridRow = (int)(rel - scrollbackLines);
    if (gridRow < GetCursorPos().y && gridRow < Rows()) {
        RefreshRowCache();
        return &rowCache[gridRow];
    }
    return std::monostate{};
}

Point TerminalScreen::GetCursorPos() const {
    auto cursor = terminal->GetCursor();
    return {cursor.x, cursor.y};
}

bool TerminalScreen::IsCursorVisible() const {
    return terminal->GetCursor().isVisible;
}

int TerminalScreen::Cols() const {
    return isSized ? terminal->Cols() : 0;
}

int TerminalScreen::Rows() const {
    return isSized ? terminal->Rows() : 0;
}

//
// gnilk::vt::ITerminalHost
//
void TerminalScreen::OnRowScrolledOut(std::span<const gnilk::vt::Cell> row, bool isWrapped) {
    scrollback->AddLine(RowToLine(ResolveRow(row)));
    TrimScrollbackIfNeeded();
}

void TerminalScreen::WriteToPty(std::string_view bytes) {
    if (host != nullptr) {
        host->WriteToPty(bytes);
    }
}

// OSC 10/11/12 and OSC 4: the colours cells resolve to
bool TerminalScreen::QueryColor(int which, gnilk::vt::Rgb &out) {
    ColorRGBA color;
    if ((which == 10) || (which == 12)) {
        color = defaultFg;
    } else if (which == 11) {
        color = defaultBg;
    } else if ((which >= 0) && (which < 256)) {
        color = palette[which];
    } else {
        return false;
    }
    auto channel = [](float value) {
        return (uint8_t)std::clamp((int)std::lround(value * 255.0f), 0, 255);
    };
    out = {channel(color.R()), channel(color.G()), channel(color.B())};
    return true;
}

void TerminalScreen::OnBell() {
    if (host != nullptr) {
        host->OnBell();
    }
}

void TerminalScreen::OnTitle(std::string_view title) {
    if (host != nullptr) {
        host->OnTitle(title);
    }
}

void TerminalScreen::OnWorkingDirectory(std::string_view uri) {
    isRowCacheStale = true;
    if (host != nullptr) {
        host->OnWorkingDirectory(uri);
    }
}

void TerminalScreen::OnSemanticPrompt(char kind, std::string_view params) {
    isRowCacheStale = true;
    if (host != nullptr) {
        host->OnSemanticPrompt(kind, params);
    }
}

void TerminalScreen::OnHyperlink(std::string_view params, std::string_view uri) {
    if (host != nullptr) {
        host->OnHyperlink(params, uri);
    }
}

void TerminalScreen::OnClipboardWrite(std::string_view selection, std::string_view base64) {
    if (host != nullptr) {
        host->OnClipboardWrite(selection, base64);
    }
}

void TerminalScreen::OnUnhandled(std::string_view description) {
    if (host != nullptr) {
        host->OnUnhandled(description);
    }
}

//
// Private
//
void TerminalScreen::RefreshRowCache() const {
    if (!isRowCacheStale) {
        return;
    }
    int cols = terminal->Cols();
    int rows = terminal->Rows();
    std::vector<gnilk::vt::Cell> cells(static_cast<size_t>(cols));
    rowCache.resize(static_cast<size_t>(rows));
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            cells[x] = terminal->At(x, y);
        }
        rowCache[y] = ResolveRow(cells);
    }
    isRowCacheStale = false;
}

TerminalScreen::Row TerminalScreen::ResolveRow(std::span<const gnilk::vt::Cell> cells) const {
    Row row;
    row.reserve(cells.size());
    for (const auto &cell : cells) {
        Cell resolved;
        resolved.ch = cell.codepoint;
        resolved.width = cell.width;
        resolved.fg = ResolveColor(cell.fg, true);
        resolved.bg = ResolveColor(cell.bg, false);
        if (cell.attrs & gnilk::vt::kAttrInverse) {
            std::swap(resolved.fg, resolved.bg);
        }
        if (cell.attrs & gnilk::vt::kAttrHidden) {
            resolved.fg = resolved.bg;
        }
        if (cell.attrs & gnilk::vt::kAttrBold)      { resolved.attrs |= kAttrBold; }
        if (cell.attrs & gnilk::vt::kAttrItalic)    { resolved.attrs |= kAttrItalic; }
        if (cell.attrs & gnilk::vt::kAttrUnderline) { resolved.attrs |= kAttrUnderline; }
        if (cell.attrs & gnilk::vt::kAttrStrike)    { resolved.attrs |= kAttrStrike; }
        row.push_back(resolved);
    }
    return row;
}

ColorRGBA TerminalScreen::ResolveColor(const gnilk::vt::Color &color, bool isForeground) const {
    switch (color.kind) {
        case gnilk::vt::Color::Kind::kIndexed:
            return palette[color.index];
        case gnilk::vt::Color::Kind::kRgb:
            return ColorRGBA::FromRGB((int)color.r, (int)color.g, (int)color.b);
        default:
            return isForeground ? defaultFg : defaultBg;
    }
}

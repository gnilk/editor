//
// Created by gnilk on 04.06.26.
//

#include <algorithm>
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

TerminalScreen::TerminalScreen() {
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
    if (isAltScreen) {
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
    if (isAltScreen) {
        return;   // §10
    }
    if (blocks.empty() || blocks.back().endAbsRow.has_value()) {
        return;
    }
    blocks.back().endAbsRow = AbsRowCount();
    blocks.back().exitCode  = exitCode;
}

void TerminalScreen::SetOpenBlockExit(int code) {
    if (!isAltScreen && !blocks.empty() && !blocks.back().endAbsRow.has_value()) {
        blocks.back().exitCode = code;
    }
}

void TerminalScreen::SetOpenBlockCwd(const std::filesystem::path &cwd) {
    if (!isAltScreen && !blocks.empty() && !blocks.back().endAbsRow.has_value()) {
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

void TerminalScreen::TrimScrollbackIfNeeded() {
    int cap = Config::Instance()["terminal"].GetInt("scrollback_lines", 10000);
    if (cap <= 0) {
        return;   // 0 = unlimited
    }
    if (isAltScreen) {
        // The alt-screen scrollback is ephemeral (discarded whole on RestoreScreen, §10) and is not
        // indexed by `blocks` (that deque tracks the persistent history only, never swapped out on
        // SaveScreen) - a plain line-granular trim here, no block bookkeeping.
        while ((int)scrollback->NumLines() > cap) {
            scrollback->DeleteLineAt(0);
            scrollbackBase++;
        }
        return;
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
    std::u32string text;
    text.reserve(row.size());
    for (const auto &cell : row) {
        text.push_back(cell.ch);
    }

    // Line's ctor rtrims trailing whitespace - matches every other Line in the editor and drops the
    // blank padding cells (MakeBlankRow fills the full row width) without losing any visible content.
    auto line = Line::Create(text);
    auto trimmedLen = (int)line->Length();

    // Run-length encode maximal same fg/bg/attrs spans into LineAttrib spans - the same batching
    // DrawScreenRow does for live rendering, so the converted Line reproduces the original ANSI colors.
    int x = 0;
    while (x < trimmedLen) {
        const Cell &runCell = row[x];
        Line::LineAttrib attrib;
        attrib.idxOrigString   = x;
        attrib.foregroundColor = runCell.fg;
        attrib.backgroundColor = runCell.bg;
        attrib.textAttributes  = kTextAttributes::kNormal;
        if (runCell.attrs & kAttrBold)      { attrib.textAttributes = attrib.textAttributes | kTextAttributes::kBold; }
        if (runCell.attrs & kAttrItalic)    { attrib.textAttributes = attrib.textAttributes | kTextAttributes::kItalic; }
        if (runCell.attrs & kAttrUnderline) { attrib.textAttributes = attrib.textAttributes | kTextAttributes::kUnderline; }
        if (runCell.attrs & kAttrInvert)    { attrib.textAttributes = attrib.textAttributes | kTextAttributes::kInverted; }
        line->Attributes().push_back(attrib);

        int y = x + 1;
        while (y < trimmedLen
               && row[y].fg == runCell.fg
               && row[y].bg == runCell.bg
               && row[y].attrs == runCell.attrs) {
            y++;
        }
        x = y;
    }
    return line;
}

void TerminalScreen::Resize(int newCols, int newRows) {
    int gridRows = (int)grid.size();

    // A degenerate (zero/negative) size empties the grid - this happens when the terminal pane is
    // squeezed to nothing during a window resize. cols/rows MUST track the grid: if they were left
    // non-zero (or negative) here, a subsequent grid mutator coming from the pty-reader thread (e.g.
    // EraseInLine) would pass its `cols == 0 || rows == 0` guard and write into the freed/empty grid
    // - a heap use-after-free. Forcing them to 0 makes every such guard no-op until we have a real
    // grid again.
    if (newCols <= 0 || newRows <= 0) {
        cols = 0;
        rows = 0;
        grid.clear();
        cursor = {};
        scrollRegionTop    = 0;
        scrollRegionBottom = 0;
        return;
    }
    cols = newCols;
    rows = newRows;

    std::vector<Row> newGrid(newRows, MakeBlankRow());

    // Preserve existing content across the resize rather than blanking it (the visible output - shell
    // history, the startup banner - lives in the grid until it scrolls into scrollback).
    //
    // In shell mode the live content occupies rows [0 .. cursor.y]; the rows BELOW the cursor are
    // blank padding the view never shows. So preservation is anchored to the cursor: keep the content
    // rows, and only when they overflow the new height push the topmost ones into scrollback. (The old
    // code kept the bottom `newRows` rows of the whole grid regardless of the cursor - so a short
    // prompt with blank padding below it would, on shrink, scroll the REAL content into scrollback and
    // keep the blanks, injecting blank lines that accumulated as the window was resized back and
    // forth.) In alt-screen the whole grid is content (the app repaints on resize), so treat every row
    // as content there - which reproduces the previous bottom-anchored behaviour exactly.
    int contentRows = isAltScreen ? gridRows : std::min(gridRows, cursor.y + 1);
    int srcStart = std::max(0, contentRows - newRows);
    for (int i = 0; i < srcStart; i++) {
        scrollback->AddLine(RowToLine(grid[i]));
    }
    if (srcStart > 0) {
        TrimScrollbackIfNeeded();
    }
    int count = std::min(contentRows, newRows);
    for (int i = 0; i < count; i++) {
        const Row &src = grid[srcStart + i];
        int n = std::min((int)src.size(), newCols);
        for (int x = 0; x < n; x++) {
            newGrid[i][x] = src[x];
        }
    }
    grid = std::move(newGrid);

    if (gridRows > 0) {
        cursor.y = std::clamp(cursor.y - srcStart, 0, newRows - 1);
        cursor.x = std::clamp(cursor.x, 0, newCols - 1);
    } else {
        cursor = {};
    }
    scrollRegionTop    = 0;
    scrollRegionBottom = newRows - 1;
}

void TerminalScreen::SetDefaultColors(ColorRGBA fg, ColorRGBA bg) {
    defaultFg = fg;
    defaultBg = bg;
    penFg = fg;
    penBg = bg;
}

void TerminalScreen::PutChar(char32_t ch) {
    if (cols == 0 || rows == 0) {
        return;
    }
    grid[cursor.y][cursor.x] = {ch, penFg, penBg, penAttrs};
    cursor.x++;
    if (cursor.x >= cols) {
        cursor.x = 0;
        NewLine();
    }
}

void TerminalScreen::NewLine() {
    if (rows == 0) {
        return;
    }
    if (cursor.y == scrollRegionBottom) {
        ScrollRegionUp();
    } else {
        cursor.y++;
        if (cursor.y >= rows) {
            cursor.y = rows - 1;
        }
    }
}

void TerminalScreen::CarriageReturn() {
    cursor.x = 0;
}

void TerminalScreen::SetForeground(ColorRGBA color) {
    penFg = color;
}

void TerminalScreen::SetBackground(ColorRGBA color) {
    penBg = color;
}

void TerminalScreen::ResetAttributes() {
    penFg    = defaultFg;
    penBg    = defaultBg;
    penAttrs = 0;
}

void TerminalScreen::SetAttributes(uint8_t attrs) {
    penAttrs = attrs;
}

const TerminalScreen::Row &TerminalScreen::GetRow(int y) const {
    return grid[y];
}

TextBuffer::Ref TerminalScreen::GetScrollback() const {
    return scrollback;
}

uint64_t TerminalScreen::AbsRowCount() const {
    return scrollbackBase + scrollback->NumLines() + cursor.y;
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
    if (gridRow < cursor.y && gridRow < (int)grid.size()) {
        return &grid[gridRow];
    }
    return std::monostate{};
}

Point TerminalScreen::GetCursorPos() const {
    return cursor;
}

int TerminalScreen::Cols() const {
    return cols;
}

int TerminalScreen::Rows() const {
    return rows;
}

void TerminalScreen::SetCursorPos(int x, int y) {
    if (cols == 0 || rows == 0) { return; }
    cursor.x = std::clamp(x, 0, cols - 1);
    cursor.y = std::clamp(y, 0, rows - 1);
}

void TerminalScreen::MoveCursor(int dx, int dy) {
    SetCursorPos(cursor.x + dx, cursor.y + dy);
}

void TerminalScreen::EraseToEndOfLine() {
    EraseInLine(0);
}

void TerminalScreen::EraseScreen() {
    if (cols == 0 || rows == 0) {
        return;
    }
    grid.assign(rows, MakeBlankRow());
    cursor = {};
}

void TerminalScreen::EraseInLine(int mode) {
    if (cols == 0 || rows == 0) { return; }
    auto blank = MakeBlankCell();
    switch (mode) {
        case 0: // cursor to end of line
            for (int x = cursor.x; x < cols; x++) {
                grid[cursor.y][x] = blank;
            }
            break;
        case 1: // start of line to cursor (inclusive)
            for (int x = 0; x <= cursor.x; x++) {
                grid[cursor.y][x] = blank;
            }
            break;
        case 2: // entire line
            grid[cursor.y] = MakeBlankRow();
            break;
    }
}

void TerminalScreen::EraseInDisplay(int mode) {
    if (cols == 0 || rows == 0) { return; }
    switch (mode) {
        case 0: // cursor to end of screen
            EraseInLine(0);
            for (int y = cursor.y + 1; y < rows; y++) {
                grid[y] = MakeBlankRow();
            }
            break;
        case 1: // start of screen to cursor
            for (int y = 0; y < cursor.y; y++) {
                grid[y] = MakeBlankRow();
            }
            EraseInLine(1);
            break;
        case 2: // entire screen — cursor stays (ANSI-correct; use EraseScreen() to also home cursor)
            grid.assign(rows, MakeBlankRow());
            break;
    }
}

void TerminalScreen::SaveCursorPos() {
    savedCursorPos = cursor;
}

void TerminalScreen::RestoreCursorPos() {
    cursor = savedCursorPos;
}

void TerminalScreen::SetScrollRegion(int top, int bottom) {
    if (rows == 0) { return; }
    scrollRegionTop    = std::clamp(top,    0, rows - 1);
    scrollRegionBottom = std::clamp(bottom, 0, rows - 1);
    if (scrollRegionTop >= scrollRegionBottom) {
        // Invalid region — reset to full screen
        scrollRegionTop    = 0;
        scrollRegionBottom = rows - 1;
    }
    // Setting the scroll region homes the cursor (standard terminal behaviour)
    cursor = {};
}

void TerminalScreen::SaveScreen() {
    savedGrid              = grid;
    savedScrollback        = scrollback;       // alias the real history - not mutated below
    savedScrollbackBase     = scrollbackBase;
    savedCursor            = cursor;
    savedPenFg             = penFg;
    savedPenBg             = penBg;
    savedPenAttrs          = penAttrs;
    savedScrollRegionTop    = scrollRegionTop;
    savedScrollRegionBottom = scrollRegionBottom;
    // Enter a clean alternate screen - a fresh scrollback so alt-screen output never reaches the
    // persistent (real) history; it is discarded wholesale on RestoreScreen.
    grid.assign(rows, MakeBlankRow());
    scrollback = MakeScrollbackBuffer();
    scrollbackBase = 0;
    cursor            = {};
    scrollRegionTop    = 0;
    scrollRegionBottom = rows - 1;
    isAltScreen = true;
}

void TerminalScreen::RestoreScreen() {
    if (savedGrid.empty()) {
        return;
    }
    grid               = savedGrid;
    scrollback         = savedScrollback;
    scrollbackBase      = savedScrollbackBase;
    cursor             = savedCursor;
    penFg              = savedPenFg;
    penBg              = savedPenBg;
    penAttrs           = savedPenAttrs;
    scrollRegionTop    = savedScrollRegionTop;
    scrollRegionBottom = savedScrollRegionBottom;
    savedGrid.clear();
    savedScrollback = nullptr;
    isAltScreen = false;
}

void TerminalScreen::ReverseIndex() {
    if (cols == 0 || rows == 0) { return; }
    if (cursor.y == scrollRegionTop) {
        // At top of scroll region — insert a blank line at top, bottom line of region is lost
        for (int r = scrollRegionBottom; r > scrollRegionTop; r--) {
            grid[r] = grid[r - 1];
        }
        grid[scrollRegionTop] = MakeBlankRow();
    } else {
        cursor.y = std::max(0, cursor.y - 1);
    }
}

void TerminalScreen::InsertLine(int count) {
    if (cols == 0 || rows == 0) { return; }
    count = std::clamp(count, 1, scrollRegionBottom - cursor.y + 1);
    for (int i = 0; i < count; i++) {
        for (int r = scrollRegionBottom; r > cursor.y; r--) {
            grid[r] = grid[r - 1];
        }
        grid[cursor.y] = MakeBlankRow();
    }
    cursor.x = 0;
}

void TerminalScreen::DeleteLine(int count) {
    if (cols == 0 || rows == 0) { return; }
    count = std::clamp(count, 1, scrollRegionBottom - cursor.y + 1);
    for (int i = 0; i < count; i++) {
        for (int r = cursor.y; r < scrollRegionBottom; r++) {
            grid[r] = grid[r + 1];
        }
        grid[scrollRegionBottom] = MakeBlankRow();
    }
    cursor.x = 0;
}

void TerminalScreen::InsertChar(int count) {
    if (cols == 0 || rows == 0) { return; }
    auto &row = grid[cursor.y];
    auto blank = MakeBlankCell();
    count = std::clamp(count, 1, cols - cursor.x);
    for (int i = 0; i < count; i++) {
        row.insert(row.begin() + cursor.x, blank);
        if ((int)row.size() > cols) {
            row.resize(cols);
        }
    }
}

void TerminalScreen::DeleteChar(int count) {
    if (cols == 0 || rows == 0) { return; }
    auto &row = grid[cursor.y];
    int n = std::min(count, (int)row.size() - cursor.x);
    if (n > 0) {
        row.erase(row.begin() + cursor.x, row.begin() + cursor.x + n);
        while ((int)row.size() < cols) {
            row.push_back(MakeBlankCell());
        }
    }
}

//
// Private
//

TerminalScreen::Cell TerminalScreen::MakeBlankCell() const {
    return {U' ', penFg, penBg, 0};
}

TerminalScreen::Row TerminalScreen::MakeBlankRow() const {
    return Row(cols, MakeBlankCell());
}

void TerminalScreen::ScrollRegionUp() {
    if (cols == 0 || rows == 0) {
        return;
    }
    // Only push to scrollback when the scroll region covers the top of the grid
    if (scrollRegionTop == 0) {
        scrollback->AddLine(RowToLine(grid[scrollRegionTop]));
        TrimScrollbackIfNeeded();
    }
    for (int y = scrollRegionTop; y < scrollRegionBottom; y++) {
        grid[y] = std::move(grid[y + 1]);
    }
    grid[scrollRegionBottom] = MakeBlankRow();
}

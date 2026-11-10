//
// Created by gnilk on 04.06.26.
//

#ifndef GOATEDIT_TERMINALSCREEN_H
#define GOATEDIT_TERMINALSCREEN_H

#include <array>
#include <vector>
#include <deque>
#include <memory>
#include <cstdint>
#include <variant>
#include <optional>
#include <filesystem>
#include <string_view>

#include "gvt/Terminal.h"

#include "Core/ColorRGBA.h"
#include "Core/Point.h"
#include "Core/TextBuffer.h"
#include "Core/Line.h"
#include "Core/Language/LanguageBase.h"

namespace gedit {
    //
    // The embedded terminal's model. The emulation itself - parsing, grid, cursor, modes, alternate screen - is a
    // gnilk::vt::Terminal (src/ext/gvt, docs/terminal-vt.md); this class owns what the EDITOR adds on top of it:
    // the scrollback history (rows the terminal pushes out at the top, as Lines), the command-block index over it,
    // the abs-row spine, persistence, and a row cache in resolved colours for the view (GetRow).
    //
    // Not thread-safe; the controller serialises everything under screenLock. Not copyable or movable: the
    // gvt::Terminal holds a reference back to this object as its host.
    //
    class TerminalScreen : private gnilk::vt::ITerminalHost {
    public:
        using Ref = std::shared_ptr<TerminalScreen>;

        static constexpr uint8_t kAttrBold      = 0x01;
        static constexpr uint8_t kAttrItalic    = 0x02;
        static constexpr uint8_t kAttrUnderline = 0x04;
        static constexpr uint8_t kAttrStrike    = 0x08;

        // A grid cell with its colours resolved. Inverse and hidden are already applied to fg/bg. A wide
        // character's right half is a width-0 cell with ch == 0 (draw it as nothing; skip it when reading text).
        struct Cell {
            char32_t  ch    = U' ';
            ColorRGBA fg    = {};
            ColorRGBA bg    = {};
            uint8_t   attrs = 0;
            uint8_t   width = 1;
        };

        using Row = std::vector<Cell>;

        // Resolved location of an absolute row id (see AbsRowCount/RowAtAbs): either a line in the
        // immutable scrollback, a still-live row in the grid, or empty (evicted/out of range).
        using AbsRow = std::variant<std::monostate, Line::Ref, const Row *>;

        // The block index (§3.2 of docs/terminal-scrollback.md): a line-range REFERENCE into the
        // shared scrollback - it does not own a copy. [startAbsRow, endAbsRow) resolves via RowAtAbs;
        // endAbsRow == nullopt means the block is still OPEN (the running command, always the tail of
        // `blocks`, at most one at a time).
        struct CommandBlock {
            uint64_t                 id = 0;            // monotonic, never reused
            std::u32string           command;           // empty for the implicit loose block (§7)
            uint64_t                 startAbsRow = 0;    // first output row (abs id)
            std::optional<uint64_t>  endAbsRow;          // exclusive end; nullopt while OPEN
            std::optional<int>       exitCode;           // set when known (OSC 133;D or a future probe)
            std::filesystem::path    cwd;                // best-effort, when known
            enum class Source { kCommitLine, kOsc133, kHeuristic } source = Source::kHeuristic;
            LanguageBase::Ref        language = nullptr; // optional per-block highlighter (deferred — see docs/syntax-blocks.md)
        };

    public:
        TerminalScreen();
        TerminalScreen(const TerminalScreen &) = delete;
        TerminalScreen &operator=(const TerminalScreen &) = delete;

        // The pty byte stream, any chunking (a sequence split across reads parses like a whole one)
        void Feed(const uint8_t *data, size_t length);
        void Feed(std::string_view data);

        // A zero/negative size (the pane squeezed to nothing) is ignored: the grid keeps its last real size
        // while Cols()/Rows() report 0, so nothing is drawn and no content is lost.
        void Resize(int newCols, int newRows);

        // Colours the cells resolve to: default fg/bg, and the 256-colour palette (DefaultPalette() + the theme's
        // first 16). Set before content arrives or at any time - the cache re-resolves.
        void SetDefaultColors(ColorRGBA fg, ColorRGBA bg);
        void SetPalette(const std::array<ColorRGBA, 256> &colors);
        static std::array<ColorRGBA, 256> DefaultPalette();

        // Everything gvt reports that the screen itself does not handle (replies for the pty, OSC 133/7, title,
        // bell, ...) is passed on to this host; scrollback (OnRowScrolledOut) is handled here. nullptr = dropped.
        void SetHost(gnilk::vt::ITerminalHost *newHost) { host = newHost; }

        // Rendering accessors
        const Row &GetRow(int y) const;
        TextBuffer::Ref GetScrollback() const;
        Point GetCursorPos() const;
        bool IsCursorVisible() const;
        int Cols() const;
        int Rows() const;
        const gnilk::vt::Modes &GetModes() const { return terminal->GetModes(); }

        // Abs-line spine (§3.1 of docs/terminal-scrollback.md): a monotonic id space spanning
        // scrollback lines then live grid history rows, stable across scrollback eviction.
        uint64_t ScrollbackBase() const { return scrollbackBase; }
        uint64_t AbsRowCount() const;             // == absBottom (the live/cursor row's abs id)
        AbsRow RowAtAbs(uint64_t abs) const;

        // Alternate screen (a full-screen app owns the grid; nothing reaches scrollback meanwhile)
        bool IsAltScreen() const { return terminal->IsAltScreen(); }

        // The block index (§3.2/§4 of docs/terminal-scrollback.md). All called under screenLock by the
        // controller; no-ops while IsAltScreen() (§10 - alt-screen rows are never grouped into blocks).
        // Every scrollback line belongs to some block (§7) - including a leading, command-less "loose"
        // block (Source::kHeuristic) that the constructor opens up front, so eviction can always assume
        // "drop the oldest whole block" rather than ever orphaning lines.
        void BeginCommandBlock(const std::u32string &command, CommandBlock::Source src);
        void EndOpenBlock(std::optional<int> exitCode);
        void SetOpenBlockExit(int code);
        void SetOpenBlockCwd(const std::filesystem::path &cwd);
        const std::deque<CommandBlock> &Blocks() const { return blocks; }

        // Resolve a block's output to text, one string per row (TS-2a, §9). Walks [startAbsRow,
        // endAbsRow) via RowAtAbs - scrollback Lines plus the live grid tail; the OPEN block resolves
        // up to the live cursor row (endAbsRow defaults to AbsRowCount()). Rows that have been evicted
        // resolve empty and are skipped (eviction is whole-block per §7, so a retained block is always
        // complete - this only bites a block caught mid-eviction). Returns empty if no block has the
        // given id. Pure read; the caller holds screenLock.
        std::vector<std::u32string> GetBlockOutputText(uint64_t blockId) const;

        // --- Persistence (terminal-scrollback.md §8) --------------------------------------------------
        // One persisted block, with its line range expressed as indices into a ScrollbackSnapshot's
        // `lines` (NOT abs-row ids). The cross-boundary value type between the model and the session DTO
        // (TerminalBlockSession): it carries no Phase-4 `language` Ref and never aliases the live grid.
        struct PersistedBlock {
            uint64_t                id = 0;
            std::u32string          command;
            uint64_t                startLine = 0;          // inclusive, index into ScrollbackSnapshot::lines
            uint64_t                endLine = 0;            // exclusive
            std::optional<int>      exitCode;               // nullopt = unknown (open block closed at tail)
            std::filesystem::path   cwd;
            CommandBlock::Source    source = CommandBlock::Source::kHeuristic;
        };
        struct ScrollbackSnapshot {
            std::vector<Line::Ref>      lines;              // immutable scrollback lines (with ANSI colours)
            std::vector<PersistedBlock> blocks;             // ranges re-based into [0, lines.size()]
        };

        // Snapshot the last `maxLines` immutable scrollback lines plus the command blocks intersecting
        // them, each block RE-BASED into the returned slice's index space [0, lines.size()] (§8.1).
        // maxLines == 0 keeps all scrollback. The live grid is NOT included: the open/running block is
        // returned closed at the saved tail (its live-grid rows dropped, exitCode left unset). Blocks
        // lying entirely outside the slice are omitted. Returns an empty snapshot while IsAltScreen()
        // (§10 - alt-screen history is never persisted) or when nothing has scrolled into scrollback.
        // Pure read; the caller holds screenLock.
        ScrollbackSnapshot SnapshotScrollbackTail(size_t maxLines) const;

        // Seed the immutable scrollback + block index from a persisted snapshot, BEFORE the shell starts
        // producing output (§8.2 - the pty thread writes immediately). Replaces any existing scrollback
        // (scrollbackBase becomes 0; seeded lines occupy abs ids [0, lines.size())) and the block index.
        // `seedBlocks` carry re-based line indices in [0, lines.size()]; they are inserted CLOSED and a
        // fresh OPEN loose block is appended to cover post-restore output (keeps §7's "every scrollback
        // line belongs to some block"). nextBlockId advances past every seeded id. No-op if `lines` is
        // empty. Caller holds screenLock; must run before shell.Begin().
        void SeedScrollback(const std::vector<Line::Ref> &lines, const std::vector<PersistedBlock> &seedBlocks);

    private:
        // gnilk::vt::ITerminalHost - OnRowScrolledOut feeds scrollback; the rest goes on to `host`
        void OnRowScrolledOut(std::span<const gnilk::vt::Cell> row, bool isWrapped) override;
        void WriteToPty(std::string_view bytes) override;
        bool QueryColor(int which, gnilk::vt::Rgb &out) override;
        void OnBell() override;
        void OnTitle(std::string_view title) override;
        void OnWorkingDirectory(std::string_view uri) override;
        void OnSemanticPrompt(char kind, std::string_view params) override;
        void OnHyperlink(std::string_view params, std::string_view uri) override;
        void OnClipboardWrite(std::string_view selection, std::string_view base64) override;
        void OnUnhandled(std::string_view description) override;

        // gvt cells -> the editor's resolved Cells (colour references through the palette, inverse/hidden applied)
        Row ResolveRow(std::span<const gnilk::vt::Cell> cells) const;
        ColorRGBA ResolveColor(const gnilk::vt::Color &color, bool isForeground) const;
        void RefreshRowCache() const;

        // Lossless Row -> Line conversion (run-length encodes maximal same fg/bg/attrs spans into
        // LineAttrib spans, the same batching DrawScreenRow already does for live rendering) used at
        // every point a row leaves the live grid and becomes immutable scrollback history. Trailing
        // blank cells are trimmed (Line's own ctor rtrims), matching how every other Line in the
        // editor is stored. A wide character's right half (width 0) contributes nothing.
        Line::Ref RowToLine(const Row &row) const;

        static TextBuffer::Ref MakeScrollbackBuffer();

        // Cap enforcement (§7 of docs/terminal-scrollback.md): "terminal.scrollback_lines" config,
        // default 10000, 0 = unlimited. Block-granular: evicts the oldest WHOLE block (its lines +
        // its CommandBlock entry together) so a retained block is always complete - never behead the
        // oldest surviving block line-by-line. Called right after every scrollback->AddLine() so the
        // buffer never grows past the cap.
        void TrimScrollbackIfNeeded();

    private:
        std::unique_ptr<gnilk::vt::Terminal> terminal;
        gnilk::vt::ITerminalHost *host = nullptr;
        bool isSized = false;           // false until the first real Resize, and while squeezed to nothing

        // GetRow's cache of the grid in resolved colours. Marked stale on every Feed/Resize/colour change - and
        // before each host callback, which runs mid-Feed and may read the grid (OSC 133 reads the command text).
        mutable std::vector<Row> rowCache;
        mutable bool isRowCacheStale = true;

        ColorRGBA defaultFg = {};
        ColorRGBA defaultBg = {};
        std::array<ColorRGBA, 256> palette;

        // Immutable scrollback history. Fed by RowToLine() whenever a row scrolls off the top of the
        // grid. Read-only, no language attached (plain ANSI-colored lines, no async tokenizer).
        TextBuffer::Ref scrollback;
        uint64_t scrollbackBase = 0;   // abs id of scrollback line 0 (bumped on eviction, §7)

        // Block index (§3.2) - deque so the oldest pops cheaply on eviction. At most one open block
        // (endAbsRow == nullopt) at the tail. Never empty: the ctor opens the initial loose block.
        std::deque<CommandBlock> blocks;
        uint64_t nextBlockId = 0;

    };
}

#endif //GOATEDIT_TERMINALSCREEN_H

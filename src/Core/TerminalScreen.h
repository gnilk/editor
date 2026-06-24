//
// Created by gnilk on 04.06.26.
//

#ifndef GOATEDIT_TERMINALSCREEN_H
#define GOATEDIT_TERMINALSCREEN_H

#include <vector>
#include <deque>
#include <memory>
#include <cstdint>
#include <variant>
#include <optional>
#include <filesystem>

#include "Core/ColorRGBA.h"
#include "Core/Point.h"
#include "Core/TextBuffer.h"
#include "Core/Line.h"
#include "Core/Language/LanguageBase.h"

namespace gedit {
    class TerminalScreen {
    public:
        using Ref = std::shared_ptr<TerminalScreen>;

        static constexpr uint8_t kAttrBold      = 0x01;
        static constexpr uint8_t kAttrItalic    = 0x02;
        static constexpr uint8_t kAttrUnderline = 0x04;
        static constexpr uint8_t kAttrInvert    = 0x08;

        struct Cell {
            char32_t  ch    = U' ';
            ColorRGBA fg    = {};
            ColorRGBA bg    = {};
            uint8_t   attrs = 0;
        };

        using Row = std::vector<Cell>;

        // Resolved location of an absolute row id (see AbsRowCount/RowAtAbs): either a line in the
        // immutable scrollback, a still-live row in the grid, or empty (evicted/out of range).
        using AbsRow = std::variant<std::monostate, Line::Ref, const Row *>;

        // The block index (§3.2 of docs/partially_done/terminal-scrollback.md): a line-range REFERENCE into the
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
            LanguageBase::Ref        language = nullptr; // optional per-block highlighter (§3.4, Phase 4)
        };

    public:
        TerminalScreen();

        void Resize(int newCols, int newRows);
        void SetDefaultColors(ColorRGBA fg, ColorRGBA bg);

        // Step 1 — stream input (replaces historyBuffer + lastLine)
        void PutChar(char32_t ch);
        void NewLine();
        void CarriageReturn();

        // Pen state
        void SetForeground(ColorRGBA color);
        void SetBackground(ColorRGBA color);
        void ResetAttributes();

        // Rendering accessors
        const Row &GetRow(int y) const;
        TextBuffer::Ref GetScrollback() const;
        Point GetCursorPos() const;
        int Cols() const;
        int Rows() const;

        // Abs-line spine (§3.1 of docs/partially_done/terminal-scrollback.md): a monotonic id space spanning
        // scrollback lines then live grid history rows, stable across scrollback eviction.
        uint64_t ScrollbackBase() const { return scrollbackBase; }
        uint64_t AbsRowCount() const;             // == absBottom (the live/cursor row's abs id)
        AbsRow RowAtAbs(uint64_t abs) const;

        // Step 2 — cursor movement and erase (full-screen apps)
        void SetCursorPos(int x, int y);
        void MoveCursor(int dx, int dy);
        void EraseToEndOfLine();   // convenience — same as EraseInLine(0)
        void EraseScreen();        // convenience — clears grid AND resets cursor to (0,0)
        void EraseInLine(int mode);     // 0=to end, 1=from start, 2=entire line
        void EraseInDisplay(int mode);  // 0=to end, 1=from start, 2=entire (cursor stays)
        void SetAttributes(uint8_t attrs);

        // Step 3 — full-screen app line/char operations
        void ReverseIndex();           // ESC M: scroll down or move cursor up
        void InsertLine(int count);    // CSI L: insert blank lines at cursor row
        void DeleteLine(int count);    // CSI M: delete lines at cursor row
        void InsertChar(int count);    // CSI @: insert blank chars at cursor col
        void DeleteChar(int count);    // CSI P: delete chars at cursor col

        // Cursor save/restore (position only, not full grid)
        void SaveCursorPos();
        void RestoreCursorPos();

        // Scroll region (0-indexed; defaults to full grid on Resize)
        void SetScrollRegion(int top, int bottom);
        int ScrollRegionTop()    const { return scrollRegionTop;    }
        int ScrollRegionBottom() const { return scrollRegionBottom; }

        // Alternate screen buffer
        void SaveScreen();
        void RestoreScreen();
        bool IsAltScreen() const { return isAltScreen; }

        // The block index (§3.2/§4 of docs/partially_done/terminal-scrollback.md). All called under screenLock by the
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

    private:
        Cell MakeBlankCell() const;
        Row  MakeBlankRow() const;
        void ScrollRegionUp();

        // Lossless Row -> Line conversion (run-length encodes maximal same fg/bg/attrs spans into
        // LineAttrib spans, the same batching DrawScreenRow already does for live rendering) used at
        // every point a row leaves the live grid and becomes immutable scrollback history. Trailing
        // blank cells are trimmed (Line's own ctor rtrims), matching how every other Line in the
        // editor is stored.
        Line::Ref RowToLine(const Row &row) const;

        static TextBuffer::Ref MakeScrollbackBuffer();

        // Cap enforcement (§7 of docs/partially_done/terminal-scrollback.md): "terminal.scrollback_lines" config,
        // default 10000, 0 = unlimited. Block-granular: evicts the oldest WHOLE block (its lines +
        // its CommandBlock entry together) so a retained block is always complete - never behead the
        // oldest surviving block line-by-line. Called right after every scrollback->AddLine() so the
        // buffer never grows past the cap.
        void TrimScrollbackIfNeeded();

    private:
        int cols = 0;
        int rows = 0;

        std::vector<Row> grid;

        // Immutable scrollback history. Fed by RowToLine() whenever a row scrolls off the top of the
        // grid. Read-only, no language attached (plain ANSI-colored lines, no async tokenizer).
        TextBuffer::Ref scrollback;
        uint64_t scrollbackBase = 0;   // abs id of scrollback line 0 (bumped on eviction, §7)

        // Block index (§3.2) - deque so the oldest pops cheaply on eviction. At most one open block
        // (endAbsRow == nullopt) at the tail. Never empty: the ctor opens the initial loose block.
        std::deque<CommandBlock> blocks;
        uint64_t nextBlockId = 0;

        Point     cursor     = {};
        ColorRGBA penFg      = {};
        ColorRGBA penBg      = {};
        uint8_t   penAttrs   = 0;

        ColorRGBA defaultFg  = {};
        ColorRGBA defaultBg  = {};

        int scrollRegionTop    = 0;
        int scrollRegionBottom = 0;

        // Saved cursor position (SaveCursorPos/RestoreCursorPos)
        Point savedCursorPos = {};

        bool isAltScreen = false;

        // Saved state for alternate screen
        std::vector<Row>     savedGrid;
        TextBuffer::Ref       savedScrollback;
        uint64_t              savedScrollbackBase = 0;
        Point                savedCursor   = {};
        ColorRGBA            savedPenFg    = {};
        ColorRGBA            savedPenBg    = {};
        uint8_t              savedPenAttrs = 0;
        int                  savedScrollRegionTop    = 0;
        int                  savedScrollRegionBottom = 0;
    };
}

#endif //GOATEDIT_TERMINALSCREEN_H

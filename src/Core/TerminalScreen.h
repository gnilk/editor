//
// Created by gnilk on 04.06.26.
//

#ifndef GOATEDIT_TERMINALSCREEN_H
#define GOATEDIT_TERMINALSCREEN_H

#include <array>
#include <vector>
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

namespace gedit {
    //
    // The embedded terminal's model. The emulation itself - parsing, grid, cursor, modes, alternate screen - is a
    // gnilk::vt::Terminal (src/ext/gvt, docs/terminal-vt.md); this class owns what the EDITOR adds on top of it:
    // the scrollback history (rows the terminal pushes out at the top, as Lines), where the last command's output
    // starts, the abs-row spine, persistence, and a row cache in resolved colours for the view (GetRow).
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

        // What the view draws of the grid: its rows (resolved), the cursor and where the grid starts in the abs-row
        // spine. CaptureFrame takes the live one; GetHeldFrame is the one taken when the app began a synchronized
        // update (?2026), shown until it ends.
        struct Frame {
            std::vector<Row> rows;
            Point cursor = {};
            bool isCursorVisible = true;
            gnilk::vt::CursorShape cursorShape = gnilk::vt::CursorShape::kDefault;
            bool isAltScreen = false;
            uint64_t gridTopAbs = 0;        // abs id of rows[0]; older rows resolve via RowAtAbs

            const Row &GetRow(int y) const {
                static const Row empty;
                return ((y >= 0) && (y < (int)rows.size())) ? rows[y] : empty;
            }
        };

        // Resolved location of an absolute row id (see AbsRowCount/RowAtAbs): either a line in the
        // immutable scrollback, a still-live row in the grid, or empty (evicted/out of range).
        using AbsRow = std::variant<std::monostate, Line::Ref, const Row *>;

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

        Frame CaptureFrame() const;
        // The frame from just before the current synchronized update; null when there is none
        const Frame *GetHeldFrame() const;

        // The last command's output (a stack trace to copy): MarkOutputStart is called when a command is committed -
        // its output starts on the row below the cursor (the command line) - or, with shell integration, at OSC
        // 133;C, where it starts on the cursor row. GetLastOutputText is that output, one string per row, up to the
        // cursor row (the next prompt); empty when there is none, or it has been trimmed or cleared away.
        void MarkOutputStart(bool isOnCursorRow);
        std::vector<std::u32string> GetLastOutputText() const;

        // --- Persistence (terminal-scrollback.md §8) --------------------------------------------------
        // The last `maxLines` scrollback lines (0 = all); the live grid is not included. Empty on the alternate
        // screen (§10). Pure read; the caller holds screenLock.
        std::vector<Line::Ref> SnapshotScrollbackTail(size_t maxLines) const;

        // Replace the scrollback with persisted lines, BEFORE the shell starts producing output (§8.2 - the pty
        // thread writes immediately); they take abs ids [0, lines.size()). No-op if `lines` is empty.
        void SeedScrollback(const std::vector<Line::Ref> &lines);

    private:
        // gnilk::vt::ITerminalHost - OnRowScrolledOut feeds scrollback; the rest goes on to `host`
        void OnRowScrolledOut(std::span<const gnilk::vt::Cell> row, bool isWrapped) override;
        void WriteToPty(std::string_view bytes) override;
        bool QueryColor(int which, gnilk::vt::Rgb &out) override;
        void OnClearScrollback() override;
        void OnSynchronizedOutput(bool isOn) override;
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

        // Cap enforcement: "terminal.scrollback_lines" config, default 10000, 0 = unlimited - the oldest lines
        // go. Called right after every scrollback->AddLine() so the buffer never grows past the cap.
        void TrimScrollbackIfNeeded();

    private:
        std::unique_ptr<gnilk::vt::Terminal> terminal;
        gnilk::vt::ITerminalHost *host = nullptr;
        bool isSized = false;           // false until the first real Resize, and while squeezed to nothing

        // GetRow's cache of the grid in resolved colours. Marked stale on every Feed/Resize/colour change - and
        // before each host callback, which runs mid-Feed and may read the grid (OSC 133 reads the command text).
        mutable std::vector<Row> rowCache;
        mutable bool isRowCacheStale = true;
        std::optional<Frame> heldFrame;

        ColorRGBA defaultFg = {};
        ColorRGBA defaultBg = {};
        std::array<ColorRGBA, 256> palette;

        // Immutable scrollback history. Fed by RowToLine() whenever a row scrolls off the top of the
        // grid. Read-only, no language attached (plain ANSI-colored lines, no async tokenizer).
        TextBuffer::Ref scrollback;
        uint64_t scrollbackBase = 0;   // abs id of scrollback line 0 (bumped on eviction, §7)
        // Abs id of the first row of the last command's output (MarkOutputStart)
        std::optional<uint64_t> lastOutputStart;

    };
}

#endif //GOATEDIT_TERMINALSCREEN_H

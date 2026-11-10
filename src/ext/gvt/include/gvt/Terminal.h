//
// gvt — the screen model: what the byte stream MEANS.
//
// Terminal implements IParserSink: it owns a Parser, interprets its events and keeps the grid (primary +
// alternate), cursor, pen, scroll region, tab stops, charsets and modes. Anything it cannot decide on its
// own (replies to the pty, bell, title, rows leaving the screen) goes to the ITerminalHost.
//
// Scope: print, C0, cursor movement, erase, insert/delete, scroll region, tab stops, save/restore, pending wrap,
// DECAWM, alternate screen, DEC Special Graphics (VT-3); SGR (VT-4). Replies and wide characters follow
// (VT-5, VT-6, docs/terminal-vt.md).
//
// Not thread-safe: the caller serialises Feed/Resize against readers (the editor holds screenLock).
//
#ifndef GVT_TERMINAL_H
#define GVT_TERMINAL_H

#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "gvt/IParserSink.h"
#include "gvt/Parser.h"

namespace gnilk::vt {

    // A colour as the application asked for it. Indexed and default colours are REFERENCES: the host
    // resolves them through its theme at draw time, so a theme change recolours what is already on screen.
    struct Color {
        enum class Kind : uint8_t {
            kDefault,
            kIndexed,
            kRgb,
        };

        Kind kind = Kind::kDefault;
        uint8_t index = 0;
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;

        static Color Indexed(uint8_t index) {
            return {Kind::kIndexed, index, 0, 0, 0};
        }
        static Color Rgb(uint8_t r, uint8_t g, uint8_t b) {
            return {Kind::kRgb, 0, r, g, b};
        }
        bool operator==(const Color &other) const = default;
    };

    enum CellAttr : uint16_t {
        kAttrNone = 0,
        kAttrBold = 1 << 0,
        kAttrDim = 1 << 1,
        kAttrItalic = 1 << 2,
        kAttrUnderline = 1 << 3,       // set whenever underlineStyle is not kNone
        kAttrBlink = 1 << 4,
        kAttrInverse = 1 << 5,
        kAttrHidden = 1 << 6,
        kAttrStrike = 1 << 7,
        kAttrOverline = 1 << 8,
    };

    // SGR 4:n (4 = single, 21 = double)
    enum class UnderlineStyle : uint8_t {
        kNone,
        kSingle,
        kDouble,
        kCurly,
        kDotted,
        kDashed,
    };

    struct Cell {
        char32_t codepoint = U' ';
        uint32_t graphemeId = 0;        // 0 = single code point (grapheme clusters: VT-6)
        Color fg = {};
        Color bg = {};
        Color underlineColor = {};      // SGR 58; default = the foreground
        uint16_t attrs = kAttrNone;
        UnderlineStyle underlineStyle = UnderlineStyle::kNone;
        uint8_t width = 1;              // 1; 2 = wide lead; 0 = wide continuation (VT-6)

        bool operator==(const Cell &other) const = default;
    };

    // What the cursor writes with (SGR state)
    struct Pen {
        Color fg = {};
        Color bg = {};
        Color underlineColor = {};
        uint16_t attrs = kAttrNone;
        UnderlineStyle underlineStyle = UnderlineStyle::kNone;
    };

    struct CursorState {
        int x = 0;
        int y = 0;
        bool isVisible = true;
        bool isPendingWrap = false;     // the last column was written; the next printable wraps first
    };

    struct Modes {
        bool isAutoWrap = true;             // DECAWM  ?7
        bool isOriginMode = false;          // DECOM   ?6
        bool isCursorKeysApp = false;       // DECCKM  ?1
        bool isKeypadApp = false;           // DECKPAM ESC = / DECKPNM ESC >
        bool isCursorVisible = true;        // DECTCEM ?25
        bool isBracketedPaste = false;      //         ?2004
        bool isInsertMode = false;          // IRM     4
        bool isNewLineMode = false;         // LNM     20 (LF also returns the carriage)
    };

    // Everything the model can't decide. All default to no-op. Called synchronously from inside Feed/Resize;
    // a callback must not call back into the Terminal that invoked it.
    class ITerminalHost {
    public:
        virtual ~ITerminalHost() = default;

        // Replies the terminal sends back (DA, DSR/CPR, ...) - VT-5
        virtual void WriteToPty(std::string_view bytes) {
        }
        // A row leaving the top of the PRIMARY screen (scroll with the region starting at the top row, or a
        // resize that pushes rows out). isWrapped: the row continues on the next one (soft wrap).
        virtual void OnRowScrolledOut(std::span<const Cell> row, bool isWrapped) {
        }
        virtual void OnBell() {
        }
        // Sequences the model recognises but does not act on; for logging
        virtual void OnUnhandled(std::string_view description) {
        }
    };

    class Terminal : public IParserSink {
    public:
        Terminal(ITerminalHost &host, int cols, int rows);

        void Feed(const uint8_t *data, size_t length);
        void Feed(std::string_view data);
        // No reflow (docs/terminal-vt.md §6). Shrinking below the cursor pushes rows out at the top.
        void Resize(int cols, int rows);
        // RIS: everything back to power-on state; the grid keeps its size
        void Reset();

        int Cols() const {
            return cols;
        }
        int Rows() const {
            return rows;
        }
        // Out-of-range coordinates return a blank cell
        const Cell &At(int x, int y) const;
        bool IsRowWrapped(int y) const;
        CursorState GetCursor() const;
        bool IsAltScreen() const {
            return isAltScreen;
        }
        const Modes &GetModes() const {
            return modes;
        }
        const Pen &GetPen() const {
            return pen;
        }
        // Bumps on every Feed/Resize/Reset that may have changed what is visible - a redraw cue
        uint64_t GetGeneration() const {
            return generation;
        }

        // IParserSink
        void Print(char32_t codepoint) override;
        void Execute(uint8_t control) override;
        void CsiDispatch(const CsiParams &params, std::string_view intermediates, uint8_t prefix,
                         uint8_t final) override;
        void EscDispatch(std::string_view intermediates, uint8_t final) override;
        void OscDispatch(std::string_view payload) override;

    private:
        enum class Charset : uint8_t {
            kAscii,
            kDecSpecialGraphics,
        };

        struct Grid {
            std::vector<Cell> cells;
            std::vector<uint8_t> isWrapped;     // per row
        };

        // DECSC / DECRC (and ?1048, ?1049): per screen
        struct SavedCursor {
            int x = 0;
            int y = 0;
            bool isPendingWrap = false;
            bool isOriginMode = false;
            Pen pen = {};
            Charset charsets[2] = {Charset::kAscii, Charset::kAscii};
            int activeCharset = 0;
        };

    private:
        void DispatchCsiPlain(const CsiParams &params, uint8_t final);
        void DispatchCsiPrivate(const CsiParams &params, uint8_t final);
        void SetMode(int mode, bool isSet);
        void SelectGraphicRendition(const CsiParams &params);
        void ApplySgr(int code);
        void ApplySgrSubParams(const CsiParams &params, size_t first, size_t last);
        size_t ParseSemicolonColor(const CsiParams &params, size_t start, Color &out) const;
        void SetUnderline(UnderlineStyle style);
        void SetSgrColor(int code, const Color &color);
        void SetPrivateMode(int mode, bool isSet);

        // Cursor
        void MoveTo(int x, int y);
        void MoveToOriginRelative(int x, int y);
        void MoveUp(int n);
        void MoveDown(int n);
        void TabForward(int n);
        void TabBackward(int n);
        void CarriageReturn();
        void LineFeed();
        void Index();
        void ReverseIndex();
        void SaveCursor();
        void RestoreCursor();

        // Grid edits
        void PutChar(char32_t codepoint);
        void ScrollUp(int top, int bottom, int n, bool isToScrollback);
        void ScrollDown(int top, int bottom, int n);
        void EraseInDisplay(int mode);
        void EraseInLine(int mode);
        void EraseCells(int y, int fromX, int toX);
        void InsertChars(int n);
        void DeleteChars(int n);
        void InsertLines(int n);
        void DeleteLines(int n);
        void ScreenAlignmentTest();

        // Screens
        void EnterAltScreen(bool isCleared);
        void LeaveAltScreen();
        void ResetState();

        Cell BlankCell() const;
        Cell &CellAt(int x, int y);
        Grid &ActiveGrid();
        const Grid &ActiveGrid() const;
        SavedCursor &ActiveSaved();
        void ClearGrid(Grid &grid);
        void ResizeGrid(Grid &grid, int newCols, int newRows, int dropTopRows);
        void ResetTabStops();
        bool IsInScrollRegion() const;
        char32_t TranslateCharset(char32_t codepoint) const;

    private:
        static constexpr int kTabWidth = 8;

        ITerminalHost &host;
        Parser parser;
        int cols;
        int rows;
        Grid primary;
        Grid alternate;
        bool isAltScreen = false;
        CursorState cursor;
        Pen pen;
        Modes modes;
        int scrollTop = 0;
        int scrollBottom = 0;           // inclusive
        std::vector<uint8_t> tabStops;  // per column
        Charset charsets[2] = {Charset::kAscii, Charset::kAscii};    // G0, G1
        int activeCharset = 0;          // SI = G0, SO = G1
        SavedCursor savedPrimary;
        SavedCursor savedAlternate;
        char32_t lastPrinted = 0;       // for REP; 0 = nothing printable yet
        uint64_t generation = 0;
    };

}

#endif // GVT_TERMINAL_H

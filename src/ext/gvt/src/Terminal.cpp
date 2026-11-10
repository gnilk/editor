//
// gvt — Terminal: the screen model behind IParserSink. See Terminal.h for scope.
//
// Behaviour follows xterm (ctlseqs) where terminals disagree; libvterm is the differential test oracle, Ghostty's
// Terminal.zig the reading reference. Every intentional difference from libvterm is listed in tests/test_oracle.cpp.
//
#include <algorithm>
#include <string>

#include "gvt/Terminal.h"

using namespace gnilk::vt;

namespace {

    // DEC Special Graphics (ESC ( 0): 0x5F-0x7E drawn as line-drawing and symbols. Same table as xterm/libvterm.
    constexpr char32_t kDecSpecialGraphics[] = {
        U' ',  // 0x5F _  blank
        U'◆',  // 0x60 `  diamond
        U'▒',  // 0x61 a  checkerboard
        U'␉',  // 0x62 b  HT
        U'␌',  // 0x63 c  FF
        U'␍',  // 0x64 d  CR
        U'␊',  // 0x65 e  LF
        U'°',  // 0x66 f  degree
        U'±',  // 0x67 g  plus/minus
        U'␤',  // 0x68 h  NL
        U'␋',  // 0x69 i  VT
        U'┘',  // 0x6A j  ┘
        U'┐',  // 0x6B k  ┐
        U'┌',  // 0x6C l  ┌
        U'└',  // 0x6D m  └
        U'┼',  // 0x6E n  ┼
        U'⎺',  // 0x6F o  scan line 1
        U'⎻',  // 0x70 p  scan line 3
        U'─',  // 0x71 q  ─
        U'⎼',  // 0x72 r  scan line 7
        U'⎽',  // 0x73 s  scan line 9
        U'├',  // 0x74 t  ├
        U'┤',  // 0x75 u  ┤
        U'┴',  // 0x76 v  ┴
        U'┬',  // 0x77 w  ┬
        U'│',  // 0x78 x  │
        U'≤',  // 0x79 y  ≤
        U'≥',  // 0x7A z  ≥
        U'π',  // 0x7B {  pi
        U'≠',  // 0x7C |  not equal
        U'£',  // 0x7D }  pound
        U'·',  // 0x7E ~  centred dot
    };

    // A count parameter: missing or 0 means 1
    int Count(const CsiParams &params, size_t index) {
        return std::max(1, params.Get(index, 1));
    }

    std::string Describe(const CsiParams &params, std::string_view intermediates, uint8_t prefix, uint8_t final) {
        std::string out = "CSI ";
        if (prefix != 0) {
            out.push_back(static_cast<char>(prefix));
        }
        for (size_t i = 0; i < params.count; i++) {
            if (i > 0) {
                out.push_back(params.IsFollowedByColon(i - 1) ? ':' : ';');
            }
            if (params.values[i] != CsiParams::kMissing) {
                out += std::to_string(params.values[i]);
            }
        }
        out += intermediates;
        out.push_back(static_cast<char>(final));
        return out;
    }

}

Terminal::Terminal(ITerminalHost &host, int cols, int rows) : host(host), parser(*this), cols(std::max(1, cols)),
                                                                rows(std::max(1, rows)) {
    ResizeGrid(primary, this->cols, this->rows, 0);
    ResizeGrid(alternate, this->cols, this->rows, 0);
    ResetState();
}

void Terminal::Feed(const uint8_t *data, size_t length) {
    if (length == 0) {
        return;
    }
    parser.Feed(data, length);
    generation++;
}

void Terminal::Feed(std::string_view data) {
    Feed(reinterpret_cast<const uint8_t *>(data.data()), data.size());
}

void Terminal::Resize(int newCols, int newRows) {
    newCols = std::max(1, newCols);
    newRows = std::max(1, newRows);
    if ((newCols == cols) && (newRows == rows)) {
        return;
    }

    // Keep the cursor's row on screen: rows above it are pushed out at the top (to the host, on the primary)
    int primaryCursorY = isAltScreen ? savedPrimary.y : cursor.y;
    int primaryDrop = std::max(0, primaryCursorY - (newRows - 1));
    int alternateDrop = isAltScreen ? std::max(0, cursor.y - (newRows - 1)) : 0;
    for (int y = 0; y < primaryDrop; y++) {
        std::span<const Cell> row(&primary.cells[static_cast<size_t>(y) * cols], static_cast<size_t>(cols));
        host.OnRowScrolledOut(row, primary.isWrapped[y] != 0);
    }
    ResizeGrid(primary, newCols, newRows, primaryDrop);
    ResizeGrid(alternate, newCols, newRows, alternateDrop);

    int activeDrop = isAltScreen ? alternateDrop : primaryDrop;
    cursor.y = std::clamp(cursor.y - activeDrop, 0, newRows - 1);
    cursor.x = std::clamp(cursor.x, 0, newCols - 1);
    cursor.isPendingWrap = false;
    savedPrimary.y = std::clamp(savedPrimary.y - primaryDrop, 0, newRows - 1);
    savedPrimary.x = std::clamp(savedPrimary.x, 0, newCols - 1);
    savedAlternate.y = std::clamp(savedAlternate.y - alternateDrop, 0, newRows - 1);
    savedAlternate.x = std::clamp(savedAlternate.x, 0, newCols - 1);

    // Tab stops: existing columns keep theirs, new columns get the default every 8
    tabStops.resize(static_cast<size_t>(newCols), 0);
    for (int x = cols; x < newCols; x++) {
        tabStops[x] = ((x % kTabWidth) == 0) ? 1 : 0;
    }

    cols = newCols;
    rows = newRows;
    scrollTop = 0;
    scrollBottom = rows - 1;
    generation++;
}

void Terminal::Reset() {
    parser.Reset();
    ResetState();
    generation++;
}

const Cell &Terminal::At(int x, int y) const {
    static const Cell blank = {};
    if ((x < 0) || (y < 0) || (x >= cols) || (y >= rows)) {
        return blank;
    }
    return ActiveGrid().cells[static_cast<size_t>(y) * cols + x];
}

bool Terminal::IsRowWrapped(int y) const {
    if ((y < 0) || (y >= rows)) {
        return false;
    }
    return ActiveGrid().isWrapped[y] != 0;
}

CursorState Terminal::GetCursor() const {
    CursorState state = cursor;
    state.isVisible = modes.isCursorVisible;
    return state;
}

//
// IParserSink
//
void Terminal::Print(char32_t codepoint) {
    PutChar(TranslateCharset(codepoint));
}

void Terminal::Execute(uint8_t control) {
    switch (control) {
        case 0x07:  // BEL
            host.OnBell();
            break;
        case 0x08:  // BS
            cursor.isPendingWrap = false;
            cursor.x = std::max(0, cursor.x - 1);
            break;
        case 0x09:  // HT
            TabForward(1);
            break;
        case 0x0A:  // LF
        case 0x0B:  // VT
        case 0x0C:  // FF
            LineFeed();
            break;
        case 0x0D:  // CR
            CarriageReturn();
            break;
        case 0x0E:  // SO
            activeCharset = 1;
            break;
        case 0x0F:  // SI
            activeCharset = 0;
            break;
        default:
            break;
    }
}

void Terminal::CsiDispatch(const CsiParams &params, std::string_view intermediates, uint8_t prefix, uint8_t final) {
    if (intermediates.empty() && (prefix == 0)) {
        DispatchCsiPlain(params, final);
        return;
    }
    if (intermediates.empty() && (prefix == '?')) {
        DispatchCsiPrivate(params, final);
        return;
    }
    // DECSTR - soft reset
    if ((intermediates == "!") && (prefix == 0) && (final == 'p')) {
        modes = Modes{};
        pen = Pen{};
        scrollTop = 0;
        scrollBottom = rows - 1;
        charsets[0] = Charset::kAscii;
        charsets[1] = Charset::kAscii;
        activeCharset = 0;
        savedPrimary = SavedCursor{};
        savedAlternate = SavedCursor{};
        cursor.isPendingWrap = false;
        return;
    }
    host.OnUnhandled(Describe(params, intermediates, prefix, final));
}

void Terminal::EscDispatch(std::string_view intermediates, uint8_t final) {
    if (intermediates.empty()) {
        switch (final) {
            case '7':
                SaveCursor();
                return;
            case '8':
                RestoreCursor();
                return;
            case 'D':   // IND
                Index();
                return;
            case 'E':   // NEL
                CarriageReturn();
                Index();
                return;
            case 'M':   // RI
                ReverseIndex();
                return;
            case 'H':   // HTS
                tabStops[cursor.x] = 1;
                return;
            case 'c':   // RIS
                ResetState();
                return;
            case '=':
                modes.isKeypadApp = true;
                return;
            case '>':
                modes.isKeypadApp = false;
                return;
            default:
                break;
        }
    } else if ((intermediates == "(") || (intermediates == ")")) {
        // Designate G0 / G1. Only DEC Special Graphics changes glyphs; every other set is treated as ASCII.
        int slot = (intermediates == "(") ? 0 : 1;
        charsets[slot] = (final == '0') ? Charset::kDecSpecialGraphics : Charset::kAscii;
        return;
    } else if ((intermediates == "#") && (final == '8')) {
        ScreenAlignmentTest();
        return;
    }
    std::string description = "ESC ";
    description += intermediates;
    description.push_back(static_cast<char>(final));
    host.OnUnhandled(description);
}

void Terminal::OscDispatch(std::string_view payload) {
    // OSC 0/2/7/8/52/133 go to the host in VT-5
    std::string description = "OSC ";
    description += payload.substr(0, payload.find(';'));
    host.OnUnhandled(description);
}

//
// CSI
//
void Terminal::DispatchCsiPlain(const CsiParams &params, uint8_t final) {
    switch (final) {
        case '@':   // ICH
            InsertChars(Count(params, 0));
            break;
        case 'A':   // CUU
            MoveUp(Count(params, 0));
            break;
        case 'B':   // CUD
        case 'e':   // VPR
            MoveDown(Count(params, 0));
            break;
        case 'C':   // CUF
        case 'a':   // HPR
            MoveTo(cursor.x + Count(params, 0), cursor.y);
            break;
        case 'D':   // CUB
            MoveTo(cursor.x - Count(params, 0), cursor.y);
            break;
        case 'E':   // CNL
            MoveDown(Count(params, 0));
            CarriageReturn();
            break;
        case 'F':   // CPL
            MoveUp(Count(params, 0));
            CarriageReturn();
            break;
        case 'G':   // CHA
        case '`':   // HPA
            MoveTo(Count(params, 0) - 1, cursor.y);
            break;
        case 'H':   // CUP
        case 'f':   // HVP
            MoveToOriginRelative(Count(params, 1) - 1, Count(params, 0) - 1);
            break;
        case 'I':   // CHT
            TabForward(Count(params, 0));
            break;
        case 'J':   // ED
            EraseInDisplay(params.Get(0, 0));
            break;
        case 'K':   // EL
            EraseInLine(params.Get(0, 0));
            break;
        case 'L':   // IL
            InsertLines(Count(params, 0));
            break;
        case 'M':   // DL
            DeleteLines(Count(params, 0));
            break;
        case 'P':   // DCH
            DeleteChars(Count(params, 0));
            break;
        case 'S':   // SU
            ScrollUp(scrollTop, scrollBottom, Count(params, 0), true);
            break;
        case 'T':   // SD (with more than one parameter it is the obsolete mouse-tracking start)
            if (params.count <= 1) {
                ScrollDown(scrollTop, scrollBottom, Count(params, 0));
            }
            break;
        case 'X':   // ECH
            cursor.isPendingWrap = false;
            EraseCells(cursor.y, cursor.x, std::min(cols - 1, cursor.x + Count(params, 0) - 1));
            break;
        case 'Z':   // CBT
            TabBackward(Count(params, 0));
            break;
        case 'b':   // REP
            if (lastPrinted != 0) {
                int n = std::min(Count(params, 0), cols * rows);
                for (int i = 0; i < n; i++) {
                    PutChar(lastPrinted);
                }
            }
            break;
        case 'd':   // VPA
            MoveToOriginRelative(cursor.x, Count(params, 0) - 1);
            break;
        case 'g':   // TBC
            if (params.Get(0, 0) == 0) {
                tabStops[cursor.x] = 0;
            } else if (params.Get(0, 0) == 3) {
                std::fill(tabStops.begin(), tabStops.end(), 0);
            }
            break;
        case 'h':   // SM
        case 'l':   // RM
            for (size_t i = 0; i < params.count; i++) {
                SetMode(params.Get(i, 0), final == 'h');
            }
            break;
        case 'm':   // SGR - VT-4; only the reset for now
            if ((params.count == 0) || ((params.count == 1) && (params.Get(0, 0) == 0))) {
                pen = Pen{};
            }
            break;
        case 'r': { // DECSTBM
            int top = Count(params, 0) - 1;
            int bottom = (params.Get(1, 0) == 0) ? rows - 1 : params.Get(1, 0) - 1;
            bottom = std::min(bottom, rows - 1);
            if (top < bottom) {
                scrollTop = top;
                scrollBottom = bottom;
                MoveToOriginRelative(0, 0);
            }
            break;
        }
        case 's':   // SCOSC (no left/right margin mode in gvt)
            SaveCursor();
            break;
        case 'u':   // SCORC
            RestoreCursor();
            break;
        default:
            host.OnUnhandled(Describe(params, {}, 0, final));
            break;
    }
}

void Terminal::DispatchCsiPrivate(const CsiParams &params, uint8_t final) {
    if ((final == 'h') || (final == 'l')) {
        for (size_t i = 0; i < params.count; i++) {
            SetPrivateMode(params.Get(i, 0), final == 'h');
        }
        return;
    }
    host.OnUnhandled(Describe(params, {}, '?', final));
}

void Terminal::SetMode(int mode, bool isSet) {
    switch (mode) {
        case 4:
            modes.isInsertMode = isSet;
            break;
        case 20:
            modes.isNewLineMode = isSet;
            break;
        default:
            host.OnUnhandled("SM/RM " + std::to_string(mode));
            break;
    }
}

void Terminal::SetPrivateMode(int mode, bool isSet) {
    switch (mode) {
        case 1:
            modes.isCursorKeysApp = isSet;
            break;
        case 6:
            modes.isOriginMode = isSet;
            MoveToOriginRelative(0, 0);
            break;
        case 7:
            modes.isAutoWrap = isSet;
            break;
        case 25:
            modes.isCursorVisible = isSet;
            break;
        case 47:
            if (isSet) {
                EnterAltScreen(false);
            } else {
                LeaveAltScreen();
            }
            break;
        case 1047:
            if (isSet) {
                EnterAltScreen(true);
            } else {
                LeaveAltScreen();
            }
            break;
        case 1048:
            if (isSet) {
                SaveCursor();
            } else {
                RestoreCursor();
            }
            break;
        case 1049:
            if (isSet) {
                if (!isAltScreen) {
                    SaveCursor();
                    EnterAltScreen(true);
                }
            } else if (isAltScreen) {
                LeaveAltScreen();
                RestoreCursor();
            }
            break;
        case 2004:
            modes.isBracketedPaste = isSet;
            break;
        default:
            host.OnUnhandled(std::string(isSet ? "DECSET " : "DECRST ") + std::to_string(mode));
            break;
    }
}

//
// Cursor
//
void Terminal::MoveTo(int x, int y) {
    cursor.x = std::clamp(x, 0, cols - 1);
    cursor.y = std::clamp(y, 0, rows - 1);
    cursor.isPendingWrap = false;
}

// CUP / VPA / DECSTBM home: with DECOM the origin is the scroll region's top and the cursor stays inside it
void Terminal::MoveToOriginRelative(int x, int y) {
    if (modes.isOriginMode) {
        MoveTo(x, std::clamp(y + scrollTop, scrollTop, scrollBottom));
        return;
    }
    MoveTo(x, y);
}

// Stops at the top margin when starting inside the region, at the screen top otherwise
void Terminal::MoveUp(int n) {
    int top = (cursor.y >= scrollTop) ? scrollTop : 0;
    MoveTo(cursor.x, std::max(top, cursor.y - n));
}

void Terminal::MoveDown(int n) {
    int bottom = (cursor.y <= scrollBottom) ? scrollBottom : rows - 1;
    MoveTo(cursor.x, std::min(bottom, cursor.y + n));
}

void Terminal::TabForward(int n) {
    int x = cursor.x;
    for (int i = 0; (i < n) && (x < cols - 1); i++) {
        x++;
        while ((x < cols - 1) && (tabStops[x] == 0)) {
            x++;
        }
    }
    cursor.x = x;
    cursor.isPendingWrap = false;
}

void Terminal::TabBackward(int n) {
    int x = cursor.x;
    for (int i = 0; (i < n) && (x > 0); i++) {
        x--;
        while ((x > 0) && (tabStops[x] == 0)) {
            x--;
        }
    }
    cursor.x = x;
    cursor.isPendingWrap = false;
}

void Terminal::CarriageReturn() {
    cursor.x = 0;
    cursor.isPendingWrap = false;
}

void Terminal::LineFeed() {
    Index();
    if (modes.isNewLineMode) {
        CarriageReturn();
    }
}

// Down one row; at the bottom margin the region scrolls instead
void Terminal::Index() {
    cursor.isPendingWrap = false;
    if (cursor.y == scrollBottom) {
        ScrollUp(scrollTop, scrollBottom, 1, true);
    } else if (cursor.y < rows - 1) {
        cursor.y++;
    }
}

void Terminal::ReverseIndex() {
    cursor.isPendingWrap = false;
    if (cursor.y == scrollTop) {
        ScrollDown(scrollTop, scrollBottom, 1);
    } else if (cursor.y > 0) {
        cursor.y--;
    }
}

void Terminal::SaveCursor() {
    auto &saved = ActiveSaved();
    saved.x = cursor.x;
    saved.y = cursor.y;
    saved.isPendingWrap = cursor.isPendingWrap;
    saved.isOriginMode = modes.isOriginMode;
    saved.pen = pen;
    saved.charsets[0] = charsets[0];
    saved.charsets[1] = charsets[1];
    saved.activeCharset = activeCharset;
}

// Without a prior save this restores the defaults (home, default pen) - as xterm
void Terminal::RestoreCursor() {
    auto &saved = ActiveSaved();
    cursor.x = std::clamp(saved.x, 0, cols - 1);
    cursor.y = std::clamp(saved.y, 0, rows - 1);
    cursor.isPendingWrap = saved.isPendingWrap;
    modes.isOriginMode = saved.isOriginMode;
    pen = saved.pen;
    charsets[0] = saved.charsets[0];
    charsets[1] = saved.charsets[1];
    activeCharset = saved.activeCharset;
}

//
// Grid edits
//
void Terminal::PutChar(char32_t codepoint) {
    if (cursor.isPendingWrap && modes.isAutoWrap) {
        ActiveGrid().isWrapped[cursor.y] = 1;
        CarriageReturn();
        Index();
    }
    if (modes.isInsertMode) {
        InsertChars(1);
    }
    CellAt(cursor.x, cursor.y) = Cell{codepoint, 0, pen.fg, pen.bg, pen.attrs, 1};
    lastPrinted = codepoint;
    if (cursor.x < cols - 1) {
        cursor.x++;
    } else if (modes.isAutoWrap) {
        cursor.isPendingWrap = true;
    }
}

// Rows [top, bottom] move up n; blank rows enter at the bottom. With isToScrollback, rows leaving the top of the
// primary screen from row 0 go to the host first.
void Terminal::ScrollUp(int top, int bottom, int n, bool isToScrollback) {
    auto &grid = ActiveGrid();
    n = std::min(n, bottom - top + 1);
    if (isToScrollback && !isAltScreen && (top == 0)) {
        for (int y = 0; y < n; y++) {
            std::span<const Cell> row(&grid.cells[static_cast<size_t>(y) * cols], static_cast<size_t>(cols));
            host.OnRowScrolledOut(row, grid.isWrapped[y] != 0);
        }
    }
    auto rowBegin = [&](int y) {
        return grid.cells.begin() + static_cast<ptrdiff_t>(y) * cols;
    };
    std::move(rowBegin(top + n), rowBegin(bottom + 1), rowBegin(top));
    std::move(grid.isWrapped.begin() + top + n, grid.isWrapped.begin() + bottom + 1, grid.isWrapped.begin() + top);
    for (int y = bottom - n + 1; y <= bottom; y++) {
        EraseCells(y, 0, cols - 1);
    }
}

// Rows [top, bottom] move down n; blank rows enter at the top. Nothing goes to the host.
void Terminal::ScrollDown(int top, int bottom, int n) {
    auto &grid = ActiveGrid();
    n = std::min(n, bottom - top + 1);
    auto rowBegin = [&](int y) {
        return grid.cells.begin() + static_cast<ptrdiff_t>(y) * cols;
    };
    std::move_backward(rowBegin(top), rowBegin(bottom + 1 - n), rowBegin(bottom + 1));
    std::move_backward(grid.isWrapped.begin() + top, grid.isWrapped.begin() + bottom + 1 - n,
                       grid.isWrapped.begin() + bottom + 1);
    for (int y = top; y < top + n; y++) {
        EraseCells(y, 0, cols - 1);
    }
}

void Terminal::EraseInDisplay(int mode) {
    cursor.isPendingWrap = false;
    switch (mode) {
        case 0:
            EraseCells(cursor.y, cursor.x, cols - 1);
            for (int y = cursor.y + 1; y < rows; y++) {
                EraseCells(y, 0, cols - 1);
            }
            break;
        case 1:
            for (int y = 0; y < cursor.y; y++) {
                EraseCells(y, 0, cols - 1);
            }
            EraseCells(cursor.y, 0, cursor.x);
            break;
        case 2:
            for (int y = 0; y < rows; y++) {
                EraseCells(y, 0, cols - 1);
            }
            break;
        case 3:     // scrollback lives in the host
            host.OnUnhandled("ED 3");
            break;
        default:
            break;
    }
}

void Terminal::EraseInLine(int mode) {
    cursor.isPendingWrap = false;
    switch (mode) {
        case 0:
            EraseCells(cursor.y, cursor.x, cols - 1);
            break;
        case 1:
            EraseCells(cursor.y, 0, cursor.x);
            break;
        case 2:
            EraseCells(cursor.y, 0, cols - 1);
            break;
        default:
            break;
    }
}

// Blank [fromX, toX] in row y (background from the pen - BCE). Clearing through the last column ends a soft wrap.
void Terminal::EraseCells(int y, int fromX, int toX) {
    auto blank = BlankCell();
    for (int x = fromX; x <= toX; x++) {
        CellAt(x, y) = blank;
    }
    if (toX == cols - 1) {
        ActiveGrid().isWrapped[y] = 0;
    }
}

void Terminal::InsertChars(int n) {
    cursor.isPendingWrap = false;
    n = std::min(n, cols - cursor.x);
    auto rowStart = ActiveGrid().cells.begin() + static_cast<ptrdiff_t>(cursor.y) * cols;
    std::move_backward(rowStart + cursor.x, rowStart + (cols - n), rowStart + cols);
    auto blank = BlankCell();
    std::fill(rowStart + cursor.x, rowStart + cursor.x + n, blank);
}

void Terminal::DeleteChars(int n) {
    cursor.isPendingWrap = false;
    n = std::min(n, cols - cursor.x);
    auto rowStart = ActiveGrid().cells.begin() + static_cast<ptrdiff_t>(cursor.y) * cols;
    std::move(rowStart + cursor.x + n, rowStart + cols, rowStart + cursor.x);
    auto blank = BlankCell();
    std::fill(rowStart + (cols - n), rowStart + cols, blank);
}

// IL / DL act only with the cursor inside the scroll region, and leave it in the first column
void Terminal::InsertLines(int n) {
    if (!IsInScrollRegion()) {
        return;
    }
    ScrollDown(cursor.y, scrollBottom, n);
    CarriageReturn();
}

void Terminal::DeleteLines(int n) {
    if (!IsInScrollRegion()) {
        return;
    }
    ScrollUp(cursor.y, scrollBottom, n, false);
    CarriageReturn();
}

// DECALN: fill with 'E', margins reset, cursor home
void Terminal::ScreenAlignmentTest() {
    auto &grid = ActiveGrid();
    std::fill(grid.cells.begin(), grid.cells.end(), Cell{U'E'});
    std::fill(grid.isWrapped.begin(), grid.isWrapped.end(), 0);
    scrollTop = 0;
    scrollBottom = rows - 1;
    MoveTo(0, 0);
}

//
// Screens
//
void Terminal::EnterAltScreen(bool isCleared) {
    if (isAltScreen) {
        return;
    }
    isAltScreen = true;
    if (isCleared) {
        ClearGrid(alternate);
    }
}

void Terminal::LeaveAltScreen() {
    isAltScreen = false;
}

void Terminal::ResetState() {
    ClearGrid(primary);
    ClearGrid(alternate);
    isAltScreen = false;
    cursor = CursorState{};
    pen = Pen{};
    modes = Modes{};
    scrollTop = 0;
    scrollBottom = rows - 1;
    ResetTabStops();
    charsets[0] = Charset::kAscii;
    charsets[1] = Charset::kAscii;
    activeCharset = 0;
    savedPrimary = SavedCursor{};
    savedAlternate = SavedCursor{};
    lastPrinted = 0;
}

//
// Helpers
//
Cell Terminal::BlankCell() const {
    Cell blank;
    blank.bg = pen.bg;
    return blank;
}

Cell &Terminal::CellAt(int x, int y) {
    return ActiveGrid().cells[static_cast<size_t>(y) * cols + x];
}

Terminal::Grid &Terminal::ActiveGrid() {
    return isAltScreen ? alternate : primary;
}

const Terminal::Grid &Terminal::ActiveGrid() const {
    return isAltScreen ? alternate : primary;
}

Terminal::SavedCursor &Terminal::ActiveSaved() {
    return isAltScreen ? savedAlternate : savedPrimary;
}

void Terminal::ClearGrid(Grid &grid) {
    std::fill(grid.cells.begin(), grid.cells.end(), BlankCell());
    std::fill(grid.isWrapped.begin(), grid.isWrapped.end(), 0);
}

// Top-left aligned copy into the new size, skipping the first dropTopRows rows
void Terminal::ResizeGrid(Grid &grid, int newCols, int newRows, int dropTopRows) {
    Grid resized;
    resized.cells.assign(static_cast<size_t>(newCols) * newRows, Cell{});
    resized.isWrapped.assign(static_cast<size_t>(newRows), 0);
    int oldRows = grid.isWrapped.size();
    int copyCols = std::min(cols, newCols);
    for (int y = 0; (y < newRows) && (y + dropTopRows < oldRows); y++) {
        auto src = grid.cells.begin() + static_cast<ptrdiff_t>(y + dropTopRows) * cols;
        std::copy(src, src + copyCols, resized.cells.begin() + static_cast<ptrdiff_t>(y) * newCols);
        // A row cut narrower no longer flows into the next one
        resized.isWrapped[y] = (newCols >= cols) ? grid.isWrapped[y + dropTopRows] : 0;
    }
    grid = std::move(resized);
}

void Terminal::ResetTabStops() {
    tabStops.assign(static_cast<size_t>(cols), 0);
    for (int x = 0; x < cols; x += kTabWidth) {
        tabStops[x] = 1;
    }
}

bool Terminal::IsInScrollRegion() const {
    return (cursor.y >= scrollTop) && (cursor.y <= scrollBottom);
}

char32_t Terminal::TranslateCharset(char32_t codepoint) const {
    if ((charsets[activeCharset] == Charset::kDecSpecialGraphics) && (codepoint >= 0x5F) && (codepoint <= 0x7E)) {
        return kDecSpecialGraphics[codepoint - 0x5F];
    }
    return codepoint;
}

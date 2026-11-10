//
// gvt — Terminal: the screen model behind IParserSink. See Terminal.h for scope.
//
// Behaviour follows xterm (ctlseqs) where terminals disagree; libvterm is the differential test oracle, Ghostty's
// Terminal.zig the reading reference. Every intentional difference from libvterm is listed in tests/test_oracle.cpp.
//
#include <algorithm>
#include <string>
#include <cstdio>

#include "gvt/Terminal.h"
#include "gvt/Version.h"
#include "gvt/CharWidth.h"

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

std::u32string Terminal::Grapheme(const Cell &cell) const {
    std::u32string text(1, cell.codepoint);
    if ((cell.graphemeId != 0) && (cell.graphemeId <= graphemes.size())) {
        text += graphemes[cell.graphemeId - 1];
    }
    return text;
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
    // ':' sub-parameters only mean something to SGR; anywhere else the sequence is dropped (as Ghostty's parser does)
    if ((params.colonMask != 0) && !((final == 'm') && intermediates.empty() && (prefix == 0))) {
        host.OnUnhandled(Describe(params, intermediates, prefix, final));
        return;
    }
    if (intermediates.empty() && (prefix == 0)) {
        DispatchCsiPlain(params, final);
        return;
    }
    if (intermediates.empty() && (prefix == '?')) {
        DispatchCsiPrivate(params, final);
        return;
    }
    // DA2 - VT220-class, firmware 10
    if (intermediates.empty() && (prefix == '>') && (final == 'c') && (params.Get(0, 0) == 0)) {
        host.WriteToPty("\x1b[>1;10;0c");
        return;
    }
    // XTVERSION
    if (intermediates.empty() && (prefix == '>') && (final == 'q') && (params.Get(0, 0) == 0)) {
        host.WriteToPty("\x1bP>|gvt " + std::to_string(kVersionMajor) + "." + std::to_string(kVersionMinor) + "." +
                        std::to_string(kVersionPatch) + "\x1b\\");
        return;
    }
    // DECRQM - "is mode n set?"
    if ((intermediates == "$") && (final == 'p') && ((prefix == 0) || (prefix == '?'))) {
        ReportMode(params.Get(0, 0), prefix == '?');
        return;
    }
    // DECSCUSR - cursor shape: 0/1 blinking block, 2 block, 3/4 underline, 5/6 bar (odd = blinking)
    if ((intermediates == " ") && (prefix == 0) && (final == 'q')) {
        int shape = params.Get(0, 0);
        static constexpr CursorShape kShapes[] = {CursorShape::kDefault, CursorShape::kBlock, CursorShape::kBlock,
                                                  CursorShape::kUnderline, CursorShape::kUnderline, CursorShape::kBar,
                                                  CursorShape::kBar};
        if ((shape >= 0) && (shape <= 6)) {
            modes.cursorShape = kShapes[shape];
            modes.isCursorBlink = (shape == 0) || ((shape % 2) == 1);
            return;
        }
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

// "Ps;Pt": the number decides; the rest is handed over as is
void Terminal::OscDispatch(std::string_view payload) {
    auto semicolon = payload.find(';');
    auto command = payload.substr(0, semicolon);
    auto rest = (semicolon == std::string_view::npos) ? std::string_view{} : payload.substr(semicolon + 1);

    if ((command == "0") || (command == "2")) {
        host.OnTitle(rest);
    } else if (command == "1") {
        // icon name - nothing to show it on
    } else if (command == "7") {
        host.OnWorkingDirectory(rest);
    } else if (command == "8") {
        auto split = rest.find(';');
        if (split != std::string_view::npos) {
            host.OnHyperlink(rest.substr(0, split), rest.substr(split + 1));
        }
    } else if (command == "52") {
        auto split = rest.find(';');
        if ((split != std::string_view::npos) && (rest.substr(split + 1) != "?")) {
            host.OnClipboardWrite(rest.substr(0, split), rest.substr(split + 1));
        }
    } else if (command == "133") {
        if (!rest.empty()) {
            auto params = (rest.size() > 2) ? rest.substr(2) : std::string_view{};
            host.OnSemanticPrompt(rest[0], params);
        }
    } else if (((command == "10") || (command == "11") || (command == "12")) && (rest == "?")) {
        ReplyColor(command[1] - '0' + 10, command);
    } else if (command == "4") {
        // "4;index;?" - possibly several pairs; only the queries are answered (palette changes are the theme's)
        while (!rest.empty()) {
            auto first = rest.find(';');
            if (first == std::string_view::npos) {
                break;
            }
            auto index = rest.substr(0, first);
            auto spec = rest.substr(first + 1);
            auto second = spec.find(';');
            auto value = spec.substr(0, second);
            if ((value == "?") && !index.empty() && (index.size() <= 3) &&
                (index.find_first_not_of("0123456789") == std::string_view::npos)) {
                int n = std::stoi(std::string(index));
                if (n <= 255) {
                    ReplyColor(n, "4;" + std::string(index));
                }
            }
            rest = (second == std::string_view::npos) ? std::string_view{} : spec.substr(second + 1);
        }
    } else {
        host.OnUnhandled("OSC " + std::string(command));
    }
}

// "OSC <osc>;rgb:rrrr/gggg/bbbb ST", in xterm's 16-bit form
void Terminal::ReplyColor(int which, std::string_view osc) {
    Rgb color;
    if (!host.QueryColor(which, color)) {
        return;
    }
    char reply[64];
    snprintf(reply, sizeof(reply), "\x1b]%.*s;rgb:%02x%02x/%02x%02x/%02x%02x\x1b\\", static_cast<int>(osc.size()),
             osc.data(), color.r, color.r, color.g, color.g, color.b, color.b);
    host.WriteToPty(reply);
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
        case 'm':   // SGR
            SelectGraphicRendition(params);
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
        case 'c':   // DA1 - VT220 with ANSI colour
            if (params.Get(0, 0) == 0) {
                host.WriteToPty("\x1b[?62;22c");
            }
            break;
        case 'n':   // DSR
            DeviceStatusReport(params, false);
            break;
        case 't':   // XTWINOPS: only the text-area size report (18)
            if (params.Get(0, 0) == 18) {
                host.WriteToPty("\x1b[8;" + std::to_string(rows) + ";" + std::to_string(cols) + "t");
            } else {
                host.OnUnhandled(Describe(params, {}, 0, final));
            }
            break;
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
    if (final == 'n') {
        DeviceStatusReport(params, true);
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
            } else {
                // The restore happens even when already on the primary screen (xterm: DECRC regardless)
                LeaveAltScreen();
                RestoreCursor();
            }
            break;
        case 2004:
            modes.isBracketedPaste = isSet;
            break;
        case 12:
            modes.isCursorBlink = isSet;
            break;
        case 1004:
            modes.isFocusEvents = isSet;
            break;
        case 2026:
            if (modes.isSynchronizedOutput != isSet) {
                modes.isSynchronizedOutput = isSet;
                host.OnSynchronizedOutput(isSet);
            }
            break;
        case 9:
        case 1000:
        case 1002:
        case 1003: {
            auto tracking = (mode == 9) ? MouseTracking::kX10 :
                            (mode == 1000) ? MouseTracking::kNormal :
                            (mode == 1002) ? MouseTracking::kButtonEvent : MouseTracking::kAnyEvent;
            if (isSet) {
                modes.mouseTracking = tracking;
            } else if (modes.mouseTracking == tracking) {
                modes.mouseTracking = MouseTracking::kOff;
            }
            break;
        }
        case 1005:
        case 1006:
        case 1015:
        case 1016: {
            auto encoding = (mode == 1005) ? MouseEncoding::kUtf8 :
                            (mode == 1006) ? MouseEncoding::kSgr :
                            (mode == 1015) ? MouseEncoding::kUrxvt : MouseEncoding::kSgrPixels;
            if (isSet) {
                modes.mouseEncoding = encoding;
            } else if (modes.mouseEncoding == encoding) {
                modes.mouseEncoding = MouseEncoding::kDefault;
            }
            break;
        }
        default:
            host.OnUnhandled(std::string(isSet ? "DECSET " : "DECRST ") + std::to_string(mode));
            break;
    }
}

// DECRPM: CSI [?] mode ; state $ y - state 1 set, 2 reset, 0 not recognised
void Terminal::ReportMode(int mode, bool isPrivate) {
    int state = 0;
    if (isPrivate) {
        state = PrivateModeState(mode);
    } else if (mode == 4) {
        state = modes.isInsertMode ? 1 : 2;
    } else if (mode == 20) {
        state = modes.isNewLineMode ? 1 : 2;
    }
    host.WriteToPty(std::string("\x1b[") + (isPrivate ? "?" : "") + std::to_string(mode) + ";" +
                    std::to_string(state) + "$y");
}

int Terminal::PrivateModeState(int mode) const {
    auto state = [](bool isSet) {
        return isSet ? 1 : 2;
    };
    switch (mode) {
        case 1:
            return state(modes.isCursorKeysApp);
        case 6:
            return state(modes.isOriginMode);
        case 7:
            return state(modes.isAutoWrap);
        case 12:
            return state(modes.isCursorBlink);
        case 25:
            return state(modes.isCursorVisible);
        case 47:
        case 1047:
        case 1049:
            return state(isAltScreen);
        case 9:
            return state(modes.mouseTracking == MouseTracking::kX10);
        case 1000:
            return state(modes.mouseTracking == MouseTracking::kNormal);
        case 1002:
            return state(modes.mouseTracking == MouseTracking::kButtonEvent);
        case 1003:
            return state(modes.mouseTracking == MouseTracking::kAnyEvent);
        case 1004:
            return state(modes.isFocusEvents);
        case 1005:
            return state(modes.mouseEncoding == MouseEncoding::kUtf8);
        case 1006:
            return state(modes.mouseEncoding == MouseEncoding::kSgr);
        case 1015:
            return state(modes.mouseEncoding == MouseEncoding::kUrxvt);
        case 1016:
            return state(modes.mouseEncoding == MouseEncoding::kSgrPixels);
        case 2004:
            return state(modes.isBracketedPaste);
        case 2026:
            return state(modes.isSynchronizedOutput);
        default:
            return 0;
    }
}

// DSR 5: "OK". DSR 6: cursor position (CPR), relative to the scroll region's top under DECOM; CSI ? 6 n is DECXCPR.
void Terminal::DeviceStatusReport(const CsiParams &params, bool isPrivate) {
    int request = params.Get(0, 0);
    if ((request == 5) && !isPrivate) {
        host.WriteToPty("\x1b[0n");
        return;
    }
    if (request == 6) {
        int row = cursor.y + 1 - (modes.isOriginMode ? scrollTop : 0);
        int col = cursor.x + 1;
        host.WriteToPty(std::string("\x1b[") + (isPrivate ? "?" : "") + std::to_string(row) + ";" +
                        std::to_string(col) + "R");
        return;
    }
    host.OnUnhandled(Describe(params, {}, isPrivate ? '?' : 0, 'n'));
}

//
// SGR. A parameter followed by ':' opens a sub-parameter group ("4:3", "38:2::r:g:b") that is taken as a whole;
// 38/48/58 also take the legacy ';' form ("38;5;n", "38;2;r;g;b"). A truncated ';' colour ends the sequence, as xterm.
//
void Terminal::SelectGraphicRendition(const CsiParams &params) {
    if (params.count == 0) {
        pen = Pen{};
        return;
    }
    size_t i = 0;
    while (i < params.count) {
        size_t last = i;
        while (params.IsFollowedByColon(last)) {
            last++;
        }
        if (last > i) {
            ApplySgrSubParams(params, i, last);
            i = last + 1;
            continue;
        }
        int code = params.Get(i, 0);
        if ((code == 38) || (code == 48) || (code == 58)) {
            Color color;
            size_t used = ParseSemicolonColor(params, i + 1, color);
            if (used == 0) {
                return;
            }
            SetSgrColor(code, color);
            i += 1 + used;
            continue;
        }
        ApplySgr(code);
        i++;
    }
}

void Terminal::ApplySgr(int code) {
    if ((code >= 30) && (code <= 37)) {
        pen.fg = Color::Indexed(code - 30);
        return;
    }
    if ((code >= 40) && (code <= 47)) {
        pen.bg = Color::Indexed(code - 40);
        return;
    }
    if ((code >= 90) && (code <= 97)) {
        pen.fg = Color::Indexed(code - 90 + 8);
        return;
    }
    if ((code >= 100) && (code <= 107)) {
        pen.bg = Color::Indexed(code - 100 + 8);
        return;
    }
    switch (code) {
        case 0:
            pen = Pen{};
            break;
        case 1:
            pen.attrs |= kAttrBold;
            break;
        case 2:
            pen.attrs |= kAttrDim;
            break;
        case 3:
            pen.attrs |= kAttrItalic;
            break;
        case 4:
            SetUnderline(UnderlineStyle::kSingle);
            break;
        case 5:
        case 6:
            pen.attrs |= kAttrBlink;
            break;
        case 7:
            pen.attrs |= kAttrInverse;
            break;
        case 8:
            pen.attrs |= kAttrHidden;
            break;
        case 9:
            pen.attrs |= kAttrStrike;
            break;
        case 21:
            SetUnderline(UnderlineStyle::kDouble);
            break;
        case 22:
            pen.attrs &= ~(kAttrBold | kAttrDim);
            break;
        case 23:
            pen.attrs &= ~kAttrItalic;
            break;
        case 24:
            SetUnderline(UnderlineStyle::kNone);
            break;
        case 25:
            pen.attrs &= ~kAttrBlink;
            break;
        case 27:
            pen.attrs &= ~kAttrInverse;
            break;
        case 28:
            pen.attrs &= ~kAttrHidden;
            break;
        case 29:
            pen.attrs &= ~kAttrStrike;
            break;
        case 39:
            pen.fg = Color{};
            break;
        case 49:
            pen.bg = Color{};
            break;
        case 53:
            pen.attrs |= kAttrOverline;
            break;
        case 55:
            pen.attrs &= ~kAttrOverline;
            break;
        case 59:
            pen.underlineColor = Color{};
            break;
        default:
            break;
    }
}

// params[first..last] were joined by ':'. Only 4 (underline style) and 38/48/58 (colours) take sub-parameters;
// any other group is ignored whole.
void Terminal::ApplySgrSubParams(const CsiParams &params, size_t first, size_t last) {
    int code = params.Get(first, 0);
    if (code == 4) {
        switch (params.Get(first + 1, 1)) {
            case 0:
                SetUnderline(UnderlineStyle::kNone);
                break;
            case 2:
                SetUnderline(UnderlineStyle::kDouble);
                break;
            case 3:
                SetUnderline(UnderlineStyle::kCurly);
                break;
            case 4:
                SetUnderline(UnderlineStyle::kDotted);
                break;
            case 5:
                SetUnderline(UnderlineStyle::kDashed);
                break;
            default:
                SetUnderline(UnderlineStyle::kSingle);
                break;
        }
        return;
    }
    if ((code != 38) && (code != 48) && (code != 58)) {
        return;
    }
    int kind = params.Get(first + 1, -1);
    if ((kind == 5) && (last >= first + 2)) {
        SetSgrColor(code, Color::Indexed(static_cast<uint8_t>(std::min(255, params.Get(first + 2, 0)))));
        return;
    }
    if (kind == 2) {
        // "38:2:<colorspace>:r:g:b" (ITU T.416) or the common "38:2:r:g:b"
        size_t values = last - (first + 1);
        size_t red = (values >= 4) ? first + 3 : first + 2;
        if (values < 3) {
            return;
        }
        auto channel = [&](size_t index) {
            return static_cast<uint8_t>(std::min(255, params.Get(index, 0)));
        };
        SetSgrColor(code, Color::Rgb(channel(red), channel(red + 1), channel(red + 2)));
    }
}

// "5;n" or "2;r;g;b" starting at 'start'. Returns how many parameters it took; 0 when malformed or cut short.
size_t Terminal::ParseSemicolonColor(const CsiParams &params, size_t start, Color &out) const {
    int kind = params.Get(start, -1);
    if ((kind == 5) && (start + 1 < params.count)) {
        out = Color::Indexed(static_cast<uint8_t>(std::min(255, params.Get(start + 1, 0))));
        return 2;
    }
    if ((kind == 2) && (start + 3 < params.count)) {
        auto channel = [&](size_t index) {
            return static_cast<uint8_t>(std::min(255, params.Get(index, 0)));
        };
        out = Color::Rgb(channel(start + 1), channel(start + 2), channel(start + 3));
        return 4;
    }
    return 0;
}

void Terminal::SetUnderline(UnderlineStyle style) {
    pen.underlineStyle = style;
    if (style == UnderlineStyle::kNone) {
        pen.attrs &= ~kAttrUnderline;
    } else {
        pen.attrs |= kAttrUnderline;
    }
}

void Terminal::SetSgrColor(int code, const Color &color) {
    if (code == 38) {
        pen.fg = color;
    } else if (code == 48) {
        pen.bg = color;
    } else {
        pen.underlineColor = color;
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
// One or two cells at the cursor (CharWidth); a zero-width code point joins the previous cell instead. A wide
// character that does not fit in the last column wraps first (autowrap) or lands in the last two columns.
void Terminal::PutChar(char32_t codepoint) {
    int width = CharWidth(codepoint);
    if (width == 0) {
        AttachToPrevious(codepoint);
        return;
    }
    if (width > cols) {
        width = 1;
    }
    if (cursor.isPendingWrap && modes.isAutoWrap) {
        WrapToNextRow();
    }
    if ((width == 2) && (cursor.x == cols - 1)) {
        if (modes.isAutoWrap) {
            ClearWideHalves(cursor.x, cursor.y, 1);
            CellAt(cursor.x, cursor.y) = BlankCell();
            WrapToNextRow();
        } else {
            cursor.x = cols - 2;
        }
    }
    if (modes.isInsertMode) {
        InsertChars(width);
    }
    ClearWideHalves(cursor.x, cursor.y, width);
    CellAt(cursor.x, cursor.y) = Cell{codepoint, 0, pen.fg, pen.bg, pen.underlineColor, pen.attrs,
                                      pen.underlineStyle, static_cast<uint8_t>(width)};
    if (width == 2) {
        CellAt(cursor.x + 1, cursor.y) = Cell{0, 0, pen.fg, pen.bg, pen.underlineColor, pen.attrs,
                                              pen.underlineStyle, 0};
    }
    lastPrinted = codepoint;
    if (cursor.x + width < cols) {
        cursor.x += width;
    } else {
        cursor.x = cols - 1;
        cursor.isPendingWrap = modes.isAutoWrap;
    }
}

void Terminal::WrapToNextRow() {
    ActiveGrid().isWrapped[cursor.y] = 1;
    CarriageReturn();
    Index();
}

// Combining marks, ZWJ, variation selectors: appended to the cell left of the cursor (the cursor cell itself while a
// wrap is pending). Dropped at the start of a row, where there is nothing to join.
void Terminal::AttachToPrevious(char32_t codepoint) {
    int x = cursor.isPendingWrap ? cursor.x : cursor.x - 1;
    if ((x > 0) && (CellAt(x, cursor.y).width == 0)) {
        x--;
    }
    if (x < 0) {
        return;
    }
    auto &cell = CellAt(x, cursor.y);
    if (cell.graphemeId == 0) {
        if (graphemes.size() >= kMaxGraphemes) {
            CompactGraphemes();
        }
        graphemes.emplace_back(1, codepoint);
        cell.graphemeId = static_cast<uint32_t>(graphemes.size());
    } else if (cell.graphemeId <= graphemes.size()) {
        graphemes[cell.graphemeId - 1].push_back(codepoint);
    }
}

// Writing 'width' cells at x splits any wide pair straddling either edge: its other half becomes blank
void Terminal::ClearWideHalves(int x, int y, int width) {
    if ((CellAt(x, y).width == 0) && (x > 0)) {
        CellAt(x - 1, y) = BlankCell();
    }
    int last = x + width - 1;
    if ((CellAt(last, y).width == 2) && (last + 1 < cols)) {
        CellAt(last + 1, y) = BlankCell();
    }
}

// After a row was shifted or partly erased: a lead without its continuation, or a continuation without its lead,
// becomes blank
void Terminal::RepairWideRow(Cell *row, int count, const Cell &blank) {
    for (int x = 0; x < count; x++) {
        if ((row[x].width == 2) && ((x + 1 >= count) || (row[x + 1].width != 0))) {
            row[x] = blank;
        } else if ((row[x].width == 0) && ((x == 0) || (row[x - 1].width != 2))) {
            row[x] = blank;
        }
    }
}

// The pool only grows while cells take new combining marks; past the cap it is rebuilt from the ids still on screen
void Terminal::CompactGraphemes() {
    std::vector<std::u32string> kept;
    for (auto *grid : {&primary, &alternate}) {
        for (auto &cell : grid->cells) {
            if ((cell.graphemeId != 0) && (cell.graphemeId <= graphemes.size())) {
                kept.push_back(std::move(graphemes[cell.graphemeId - 1]));
                cell.graphemeId = static_cast<uint32_t>(kept.size());
            } else {
                cell.graphemeId = 0;
            }
        }
    }
    graphemes = std::move(kept);
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
            host.OnClearScrollback();
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
    if ((fromX > 0) || (toX < cols - 1)) {
        RepairWideRow(&CellAt(0, y), cols, blank);
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
    RepairWideRow(&*rowStart, cols, blank);
}

void Terminal::DeleteChars(int n) {
    cursor.isPendingWrap = false;
    n = std::min(n, cols - cursor.x);
    auto rowStart = ActiveGrid().cells.begin() + static_cast<ptrdiff_t>(cursor.y) * cols;
    std::move(rowStart + cursor.x + n, rowStart + cols, rowStart + cursor.x);
    auto blank = BlankCell();
    std::fill(rowStart + (cols - n), rowStart + cols, blank);
    RepairWideRow(&*rowStart, cols, blank);
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
    graphemes.clear();
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
        // A row cut narrower no longer flows into the next one, and may have lost half a wide character
        resized.isWrapped[y] = (newCols >= cols) ? grid.isWrapped[y + dropTopRows] : 0;
        RepairWideRow(&resized.cells[static_cast<size_t>(y) * newCols], newCols, Cell{});
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

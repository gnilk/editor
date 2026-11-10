//
// gvt — input encoders. xterm ctlseqs: "PC-Style Function Keys" and "Mouse Tracking".
//
#include "gvt/Input.h"

using namespace gnilk::vt;

namespace {

    void AppendUtf8(std::string &out, char32_t cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0x10FFFF) {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    // "CSI 1;m X" / "SS3 X" / "CSI X" for the letter-final keys
    std::string LetterKey(char final, uint8_t modifiers, bool isSs3) {
        if (modifiers != kModNone) {
            return "\x1b[1;" + std::to_string(1 + modifiers) + final;
        }
        return std::string(isSs3 ? "\x1bO" : "\x1b[") + final;
    }

    // "CSI n ~" / "CSI n;m ~"
    std::string TildeKey(int number, uint8_t modifiers) {
        std::string out = "\x1b[" + std::to_string(number);
        if (modifiers != kModNone) {
            out += ";" + std::to_string(1 + modifiers);
        }
        out.push_back('~');
        return out;
    }

    // Ctrl with a printable ASCII character: the C0 control xterm sends, or -1 when Ctrl does not change it
    int CtrlByte(char32_t ch) {
        if ((ch >= 'a') && (ch <= 'z')) {
            return static_cast<int>(ch - 'a' + 1);
        }
        if ((ch >= '@') && (ch <= '_')) {           // @ A-Z [ \ ] ^ _
            return static_cast<int>(ch - '@');
        }
        switch (ch) {
            case ' ':
            case '2':
                return 0x00;
            case '3':
                return 0x1B;
            case '4':
                return 0x1C;
            case '5':
                return 0x1D;
            case '6':
                return 0x1E;
            case '7':
            case '/':
            case '-':
                return 0x1F;
            case '8':
            case '?':
                return 0x7F;
            default:
                return -1;
        }
    }

}

std::string gnilk::vt::EncodeKey(Key key, uint8_t modifiers, const Modes &modes) {
    std::string altPrefix = (modifiers & kModAlt) ? "\x1b" : "";
    switch (key) {
        case Key::kEnter:
            return altPrefix + "\r";
        case Key::kTab:
            if (modifiers & kModShift) {
                return "\x1b[Z";
            }
            return altPrefix + "\t";
        case Key::kBackspace:
            return altPrefix + ((modifiers & kModCtrl) ? "\x08" : "\x7f");
        case Key::kEscape:
            return altPrefix + "\x1b";
        case Key::kUp:
            return LetterKey('A', modifiers, modes.isCursorKeysApp);
        case Key::kDown:
            return LetterKey('B', modifiers, modes.isCursorKeysApp);
        case Key::kRight:
            return LetterKey('C', modifiers, modes.isCursorKeysApp);
        case Key::kLeft:
            return LetterKey('D', modifiers, modes.isCursorKeysApp);
        case Key::kHome:
            return LetterKey('H', modifiers, modes.isCursorKeysApp);
        case Key::kEnd:
            return LetterKey('F', modifiers, modes.isCursorKeysApp);
        case Key::kInsert:
            return TildeKey(2, modifiers);
        case Key::kDelete:
            return TildeKey(3, modifiers);
        case Key::kPageUp:
            return TildeKey(5, modifiers);
        case Key::kPageDown:
            return TildeKey(6, modifiers);
        case Key::kF1:
            return LetterKey('P', modifiers, true);
        case Key::kF2:
            return LetterKey('Q', modifiers, true);
        case Key::kF3:
            return LetterKey('R', modifiers, true);
        case Key::kF4:
            return LetterKey('S', modifiers, true);
        case Key::kF5:
            return TildeKey(15, modifiers);
        case Key::kF6:
            return TildeKey(17, modifiers);
        case Key::kF7:
            return TildeKey(18, modifiers);
        case Key::kF8:
            return TildeKey(19, modifiers);
        case Key::kF9:
            return TildeKey(20, modifiers);
        case Key::kF10:
            return TildeKey(21, modifiers);
        case Key::kF11:
            return TildeKey(23, modifiers);
        case Key::kF12:
            return TildeKey(24, modifiers);
    }
    return {};
}

std::string gnilk::vt::EncodeText(char32_t codepoint, uint8_t modifiers) {
    std::string out;
    if (modifiers & kModAlt) {
        out.push_back('\x1b');
    }
    if (modifiers & kModCtrl) {
        int control = CtrlByte(codepoint);
        if (control >= 0) {
            out.push_back(static_cast<char>(control));
            return out;
        }
    }
    AppendUtf8(out, codepoint);
    return out;
}

std::string gnilk::vt::EncodePaste(std::string_view text, const Modes &modes) {
    std::string body;
    body.reserve(text.size());
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\r') {
            body.push_back('\r');
            if ((i + 1 < text.size()) && (text[i + 1] == '\n')) {
                i++;
            }
        } else if (text[i] == '\n') {
            body.push_back('\r');
        } else {
            body.push_back(text[i]);
        }
    }
    if (!modes.isBracketedPaste) {
        return body;
    }
    static constexpr std::string_view kEnd = "\x1b[201~";
    for (auto pos = body.find(kEnd); pos != std::string::npos; pos = body.find(kEnd, pos)) {
        body.erase(pos, kEnd.size());
    }
    return "\x1b[200~" + body + std::string(kEnd);
}

std::string gnilk::vt::EncodeMouse(MouseButton button, MouseAction action, int x, int y, uint8_t modifiers,
                                   const Modes &modes) {
    auto tracking = modes.mouseTracking;
    if (tracking == MouseTracking::kOff) {
        return {};
    }
    bool isWheel = (button == MouseButton::kWheelUp) || (button == MouseButton::kWheelDown);
    if (action == MouseAction::kRelease && (isWheel || (tracking == MouseTracking::kX10))) {
        return {};
    }
    if (action == MouseAction::kMotion) {
        bool isReported = (tracking == MouseTracking::kAnyEvent) ||
                          ((tracking == MouseTracking::kButtonEvent) && (button != MouseButton::kNone));
        if (!isReported) {
            return {};
        }
    }

    int code = 0;
    switch (button) {
        case MouseButton::kLeft:
            code = 0;
            break;
        case MouseButton::kMiddle:
            code = 1;
            break;
        case MouseButton::kRight:
            code = 2;
            break;
        case MouseButton::kNone:
            code = 3;
            break;
        case MouseButton::kWheelUp:
            code = 64;
            break;
        case MouseButton::kWheelDown:
            code = 65;
            break;
    }
    if (action == MouseAction::kMotion) {
        code += 32;
    }
    // X10 mode reports no modifiers
    if (tracking != MouseTracking::kX10) {
        if (modifiers & kModShift) {
            code += 4;
        }
        if (modifiers & (kModAlt | kModMeta)) {
            code += 8;
        }
        if (modifiers & kModCtrl) {
            code += 16;
        }
    }

    bool isSgr = (modes.mouseEncoding == MouseEncoding::kSgr) || (modes.mouseEncoding == MouseEncoding::kSgrPixels);
    if (isSgr) {
        char final = (action == MouseAction::kRelease) ? 'm' : 'M';
        return "\x1b[<" + std::to_string(code) + ";" + std::to_string(x + 1) + ";" + std::to_string(y + 1) + final;
    }
    // X10 bytes: the release of any button is button code 3
    if (action == MouseAction::kRelease) {
        code = (code & ~3) | 3;
    }
    if ((x + 1 + 32 > 255) || (y + 1 + 32 > 255)) {
        return {};
    }
    std::string out = "\x1b[M";
    out.push_back(static_cast<char>(32 + code));
    out.push_back(static_cast<char>(32 + x + 1));
    out.push_back(static_cast<char>(32 + y + 1));
    return out;
}

std::string gnilk::vt::EncodeFocus(bool isFocused, const Modes &modes) {
    if (!modes.isFocusEvents) {
        return {};
    }
    return isFocused ? "\x1b[I" : "\x1b[O";
}

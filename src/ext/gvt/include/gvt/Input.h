//
// gvt — the other direction: what a key, a mouse event, a paste or a focus change sends to the application.
//
// Pure functions over the terminal's Modes (DECCKM, bracketed paste, mouse tracking/encoding, focus events), xterm's
// encodings (ctlseqs "PC-Style Function Keys", "Mouse Tracking"). Which input reaches the application at all is the
// host's decision; these only say what the bytes are.
//
#ifndef GVT_INPUT_H
#define GVT_INPUT_H

#include <cstdint>
#include <string>
#include <string_view>

#include "gvt/Terminal.h"

namespace gnilk::vt {

    enum class Key : uint8_t {
        kEnter,
        kTab,
        kBackspace,
        kEscape,
        kUp,
        kDown,
        kRight,
        kLeft,
        kHome,
        kEnd,
        kPageUp,
        kPageDown,
        kInsert,
        kDelete,
        kF1, kF2, kF3, kF4, kF5, kF6, kF7, kF8, kF9, kF10, kF11, kF12,
    };

    // xterm's modifier bits: the parameter sent is 1 + the sum
    enum KeyModifier : uint8_t {
        kModNone = 0,
        kModShift = 1,
        kModAlt = 2,
        kModCtrl = 4,
        kModMeta = 8,
    };

    // A non-text key. Unmodified arrows/Home/End follow DECCKM (SS3 vs CSI); modified keys use "CSI 1;m X" /
    // "CSI n;m ~". Shift+Tab is CSI Z, Alt+<key> on Enter/Tab/Backspace/Escape is ESC + the key.
    std::string EncodeKey(Key key, uint8_t modifiers, const Modes &modes);

    // A typed character (shift already applied to it). Ctrl+letter and Ctrl+symbol give the C0 control xterm sends
    // (Ctrl+Space / Ctrl+2 = NUL, Ctrl+[ = ESC, Ctrl+/ = US, ...); Alt prefixes ESC; the rest is UTF-8.
    std::string EncodeText(char32_t codepoint, uint8_t modifiers);

    // Pasted text: line breaks become CR (what Enter sends); with bracketed paste (?2004) it is wrapped in
    // ESC[200~ ... ESC[201~ and any ESC[201~ inside is removed so the paste cannot end itself early.
    std::string EncodePaste(std::string_view text, const Modes &modes);

    enum class MouseButton : uint8_t {
        kLeft,
        kMiddle,
        kRight,
        kNone,          // motion with no button down
        kWheelUp,
        kWheelDown,
    };

    enum class MouseAction : uint8_t {
        kPress,
        kRelease,
        kMotion,
    };

    // A mouse event at a 0-based cell. Empty when the current tracking mode does not report it (tracking off,
    // release in X10 mode, motion without ?1002/?1003, ...). SGR encoding (?1006) when set, else the X10 byte form
    // (coordinates past 223 cannot be expressed there - empty).
    std::string EncodeMouse(MouseButton button, MouseAction action, int x, int y, uint8_t modifiers,
                            const Modes &modes);

    // CSI I / CSI O when the application asked for focus events (?1004); empty otherwise
    std::string EncodeFocus(bool isFocused, const Modes &modes);

}

#endif // GVT_INPUT_H

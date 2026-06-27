//
// gansi input events — neutral, terminal-agnostic event types produced by InputParser. No editor
// types here; the backend translates these into the editor's KeyPress/MouseEvent.
//
#ifndef GANSI_EVENT_H
#define GANSI_EVENT_H

#include <cstdint>
#include <string>
#include <variant>

namespace gnilk::ansi {

    // Named (non-text) keys. Printable input is carried as KeyEvent::isChar instead.
    enum class Key {
        None,
        Up, Down, Left, Right,
        Home, End, PageUp, PageDown,
        Insert, Delete,
        Enter, Tab, Backspace, Escape,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    };

    // Modifier bit flags.
    enum class Mod : uint8_t {
        None  = 0,
        Shift = 1u << 0,
        Alt   = 1u << 1,
        Ctrl  = 1u << 2,
        Super = 1u << 3,
    };

    inline Mod operator|(Mod a, Mod b) {
        return static_cast<Mod>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }
    inline Mod operator&(Mod a, Mod b) {
        return static_cast<Mod>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }
    inline Mod &operator|=(Mod &a, Mod b) {
        a = a | b;
        return a;
    }
    inline bool HasMod(Mod value, Mod flag) {
        return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
    }

    // A key press. Exactly one of (isChar==true → ch) or (isChar==false → key) is meaningful.
    struct KeyEvent {
        bool isChar = false;
        char32_t ch = 0;
        Key key = Key::None;
        Mod mods = Mod::None;
    };

    struct MouseEvent {
        enum class Kind { Press, Release, Move, Wheel };
        Kind kind = Kind::Press;
        int col = 0;        // 0-based screen cell
        int row = 0;        // 0-based screen cell
        int button = 0;     // 0=left,1=middle,2=right (press/release)
        int wheel = 0;      // +1 = up/away, -1 = down/toward (Wheel kind)
        Mod mods = Mod::None;
    };

    struct PasteEvent {
        std::u32string text;
    };

    struct FocusEvent {
        bool focused = false;
    };

    struct ResizeEvent {
        int cols = 0;
        int rows = 0;
    };

    using Event = std::variant<KeyEvent, MouseEvent, PasteEvent, FocusEvent, ResizeEvent>;

}

#endif // GANSI_EVENT_H

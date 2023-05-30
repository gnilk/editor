//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_KEYBOARD_H
#define EDITOR_KEYBOARD_H

#include <string>
#include <optional>
#include <cstdint>

namespace gedit {
    class Keyboard {
    public:
        // bitmask!!
        typedef enum : uint8_t {
            kMod_None = 0,
            kMod_LeftShift = 1,
            kMod_RightShift = 2,
            kMod_LeftCtrl = 4,
            kMod_RightCtrl = 8,
            // My ext. Das Keyboard have left/right of these swapped...
            kMod_LeftAlt = 0x10,
            kMod_RightAlt = 0x20,
            kMod_LeftCommand = 0x40,
            kMod_RightCommand = 0x80,
        } kModifierKeys;

        typedef enum : uint8_t {
            kKeyCode_None = 0,
            kKeyCode_Return,
            kKeyCode_Escape,
            kKeyCode_Backspace,        // Backspace??
            kKeyCode_Tab,
            kKeyCode_Space,
            kKeyCode_F1,
            kKeyCode_F2,
            kKeyCode_F3,
            kKeyCode_F4,
            kKeyCode_F5,
            kKeyCode_F6,
            kKeyCode_F7,
            kKeyCode_F8,
            kKeyCode_F9,
            kKeyCode_F10,
            kKeyCode_F11,
            kKeyCode_F12,
            kKeyCode_PrintScreen,
            kKeyCode_ScrollLock,
            kKeyCode_Pause,
            kKeyCode_Insert,
            kKeyCode_Home,
            kKeyCode_PageUp,
            kKeyCode_DeleteForward,
            kKeyCode_End,
            kKeyCode_PageDown,
            kKeyCode_LeftArrow,
            kKeyCode_RightArrow,
            kKeyCode_DownArrow,
            kKeyCode_UpArrow,
            kKeyCode_NumLock,
            kKeyCode_MaxCodeNum,       // Don't delete this
        } kKeyCode;

        struct HWKeyEvent {
            uint8_t modifiers;                  // Any modifier flags
            kKeyCode keyCode = kKeyCode_None;   // Any special non-printable chars

            // charCode represents the RAW ASCII value, this has NOT gone through any translation what-so-ever
            // This should NOT be used unless the modifier mask is set!
            // Reason: The terminal input translates and don't forward all combinations to the application
            // it is therefore 'impossible' to read out certain combinations on certain OS:es (Linux/MacOS)
            // this applies if you are a terminal program - not if you are a UI application (Swift/X/etc..)
            uint32_t scanCode = 0;
            // This holds a basic translation of the scancode to something human readable
            // IT IS BEST EFFORT and should only be used IFF the modifiers are used and there is no other way
            // do deduce the actual combination
            uint8_t translatedScanCode;

            bool isPressedDown;       // true if this is key is pressed down..
        };
    public:
        // Make this static
        static const std::string &KeyCodeName(const Keyboard::kKeyCode keyCode);
        static std::optional<Keyboard::kKeyCode> NameToKeyCode(const std::string &name);
        static bool IsNameKeyCode(const std::string &name);

        static int ModifierMaskFromString(const std::string &strModifiers);

        static bool IsNameModifierMask(const std::string &modiferName);


    };
}
#endif //EDITOR_KEYBOARD_H

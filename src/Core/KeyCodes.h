//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_KEYCODES_H
#define EDITOR_KEYCODES_H

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
            kMod_LeftAlt = 16,
            kMod_RightAlt = 32,
            kMod_LeftCommand = 64,
            kMod_RightCommand = 128,
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


    };


/*
//
// This the old stuff
//

    typedef enum {
        kKeyCtrl_None = 0,
        kKeyCtrl_LeftShift = 1,
        kKeyCtrl_RightShift = 2,
        kKeyCtrl_LeftCtrl = 4,
        kKeyCtrl_RightCtrl = 8,
        kKeyCtrl_LeftAlt = 16,
        kKeyCtrl_RightAlt = 32,
        // Special - macOS
        kKeyCtrl_LeftCommand = (1 << 8),
        kKeyCtrl_RightCommand = (2 << 8),
//    kKeyCtrl_LeftOption   = (3<<8),       - same as left-alt
//    kKeyCtrl_RightOption  = (4<<8),       - same as right-alt
    } kStdControlKeys;

// TODO: Fix this once the mapping functionality is complete...
    typedef enum {
        kKey_NoKey_InQueue = -1,    // Special, in order to handle non-blocking

        kKey_Tab = 9,
        kKey_Return = 10,
        kKey_Escape = 27,
        kKey_Backspace = 127,   // through inspection
        kKey_ShiftTab = 353,    // Through inspection


        kKey_Delete = 400,
        kKey_Home = 401,        // Through inspection
        kKey_PageUp = 402,

        kKey_End = 404,         // through inspection
        kKey_PageDown = 405,

        // Self defined
        kKey_Left = 1024,
        kKey_Right = 1025,
        kKey_Up = 1026,
        kKey_Down = 1027,
    } kStdKeyCodes;
 */
}
#endif //EDITOR_KEYCODES_H

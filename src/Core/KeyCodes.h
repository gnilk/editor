//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_KEYCODES_H
#define EDITOR_KEYCODES_H

#include <cstdint>


typedef enum {
    kKeyCtrl_None       = 0,
    kKeyCtrl_LeftShift  = 1,
    kKeyCtrl_RightShift = 2,
    kKeyCtrl_LeftCtrl   = 4,
    kKeyCtrl_RightCtrl  = 8,
    kKeyCtrl_LeftAlt    = 16,
    kKeyCtrl_RightAlt   = 32,
    // Special - macOS
    kKeyCtrl_LeftCommand  = (1<<8),
    kKeyCtrl_RightCommand = (2<<8),
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

#endif //EDITOR_KEYCODES_H

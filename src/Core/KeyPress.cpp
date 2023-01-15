//
// Created by gnilk on 15.01.23.
//

#include "Core/KeyPress.h"
#include "Core/KeyCodes.h"

bool KeyPress::IsValid() {
    return (editorkey != kKey_NoKey_InQueue);
}

bool KeyPress::IsShiftPressed() {
    return ((data.special & kKeyCtrl_LeftShift) | (data.special & kKeyCtrl_RightShift));
}

bool KeyPress::IsHumanReadable() {
    // Human readables..  NOTE: we don't support unicode..  I'm old-skool...
    if ((data.code > 31) && (data.code < 127)) {
        return true;
    }
    return false;
}

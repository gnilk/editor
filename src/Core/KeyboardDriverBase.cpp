//
// Created by gnilk on 14.01.23.
//

#include "Core/KeyCodes.h"
#include "Core/KeyboardDriverBase.h"

KeyPress KeyboardDriverBase::GetCh() {
    return {};
}

bool KeyboardDriverBase::IsValid(KeyPress &key) {
    return (key.editorkey != kKey_NoKey_InQueue);
}

bool KeyboardDriverBase::IsShift(KeyPress &key) {
    return ((key.data.special & kKeyCtrl_LeftShift) | (key.data.special & kKeyCtrl_RightShift));
}

bool KeyboardDriverBase::IsHumanReadable(KeyPress &key) {
    // Human readables..  NOTE: we don't support unicode..  I'm old-skool...
    if ((key.data.code > 31) && (key.data.code < 127)) {
        return true;
    }
    return false;
}

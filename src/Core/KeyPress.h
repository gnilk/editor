//
// Created by gnilk on 15.01.23.
//

#ifndef EDITOR_KEYPRESS_H
#define EDITOR_KEYPRESS_H

#include <cstdint>
#include "Core/KeyCodes.h"
/*
struct KeyPress {
    union {
        int64_t editorkey;  // this is special | code
        struct {
            int32_t special;
            int32_t code;
        } data;
    };

    // This is from the underlying keyboard driver...
    int64_t rawCode;

    bool IsValid();
    bool IsShiftPressed();
    bool IsCtrlPressed();
    bool IsHumanReadable();
};
 */
namespace gedit {
    struct KeyPress {
        bool isKeyValid = false;
        bool isHwEventValid = false;
        Keyboard::HWKeyEvent hwEvent;
        uint8_t modifiers;
        int key;

        // Human readables..  NOTE: we don't support unicode..  I'm old-skool...
        bool IsHumanReadable() const {
            if (isKeyValid) {
                if ((key > 31) && (key < 127)) {
                    return true;
                }
            }
            return false;
        }
        bool IsValid() const {
            return isKeyValid;
        }
        bool IsShiftPressed() const {
            if (!isHwEventValid) return false;
            if ((modifiers & kKeyCtrl_LeftShift) || (modifiers & kKeyCtrl_RightShift)) {
                return true;
            }
            return false;
        }
        bool IsCtrlPressed() const {
            if (!isHwEventValid) return false;
            if ((modifiers & kKeyCtrl_LeftCtrl) || (modifiers & kKeyCtrl_RightCtrl)) {
                return true;
            }
            return false;
        }

    };
}


#endif //EDITOR_KEYPRESS_H

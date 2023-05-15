//
// Created by gnilk on 15.01.23.
//

#ifndef EDITOR_KEYPRESS_H
#define EDITOR_KEYPRESS_H

#include <cstdint>
#include "Core/Keyboard.h"
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
        bool isSpecialKey = false;
        Keyboard::HWKeyEvent hwEvent;
        uint8_t modifiers;
        int key;
        int specialKey;

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
            return (isKeyValid && isHwEventValid);
        }
        bool IsAnyValid() const {
            return (isKeyValid || isHwEventValid || isSpecialKey);
        }

        bool IsSpecialKeyPressed(Keyboard::kKeyCode keyCode) const {
            if (!isSpecialKey) return false;
            return (this->specialKey == keyCode);
        }

        bool IsShiftPressed() const {
//            if (!isHwEventValid) return false;
            if ((modifiers & Keyboard::kMod_LeftShift) || (modifiers & Keyboard::kMod_RightShift)) {
                return true;
            }
            return false;
        }

        bool IsCtrlPressed() const {
//            if (!isHwEventValid) return false;
            if ((modifiers & Keyboard::kMod_LeftCtrl) || (modifiers & Keyboard::kMod_RightCtrl)) {
                return true;
            }
            return false;
        }

        bool IsCommandPressed() const {
//            if (!isHwEventValid) return false;
            if ((modifiers & Keyboard::kMod_LeftCommand) || (modifiers & Keyboard::kMod_RightCommand)) {
                return true;
            }
            return false;
        }

        bool IsAltPressed() const {
//            if (!isHwEventValid) return false;
            if ((modifiers & Keyboard::kMod_LeftAlt) || (modifiers & Keyboard::kMod_RightAlt)) {
                return true;
            }
            return false;
        }

    };
}


#endif //EDITOR_KEYPRESS_H

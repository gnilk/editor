//
// Created by gnilk on 15.01.23.
//

#ifndef EDITOR_KEYPRESS_H
#define EDITOR_KEYPRESS_H

#include <cstdint>
#include <logger.h>
#include "Core/Keyboard.h"

namespace gedit {
    struct KeyPress {
        bool isKeyValid = false;
        bool isHwEventValid = false;
        bool isSpecialKey = false;      // This should denote a non-printable char, like: Keyboard::kKeyCode_PageUp
        Keyboard::HWKeyEvent hwEvent;
        uint8_t modifiers;
        char32_t key;
        int specialKey;                 // if 'isSpecialKey' is true - this contains the kKeyCode from keyboard..

        bool IsHumanReadable() const {
            if (isKeyValid) {
                if (key >= 32) {
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
        void DumpToLog();

    };
}


#endif //EDITOR_KEYPRESS_H

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
        bool isSpecialKey = false;      // Denotes a non-printable key (e.g. Keyboard::kKeyCode_PageUp)
        uint8_t modifiers = 0;
        char32_t key = 0;
        int specialKey = 0;             // Valid when isSpecialKey is true; holds a Keyboard::kKeyCode value

        bool IsHumanReadable() const {
            return isKeyValid && (key >= 32);
        }

        bool IsValid() const {
            return isKeyValid;
        }

        bool IsAnyValid() const {
            return (isKeyValid || isSpecialKey);
        }

        bool IsSpecialKeyPressed(Keyboard::kKeyCode keyCode) const {
            if (!isSpecialKey) return false;
            return (this->specialKey == keyCode);
        }

        bool IsShiftPressed() const {
            return (modifiers & Keyboard::kMod_LeftShift) || (modifiers & Keyboard::kMod_RightShift);
        }

        bool IsCtrlPressed() const {
            return (modifiers & Keyboard::kMod_LeftCtrl) || (modifiers & Keyboard::kMod_RightCtrl);
        }

        bool IsCommandPressed() const {
            return (modifiers & Keyboard::kMod_LeftCommand) || (modifiers & Keyboard::kMod_RightCommand);
        }

        bool IsAltPressed() const {
            return (modifiers & Keyboard::kMod_LeftAlt) || (modifiers & Keyboard::kMod_RightAlt);
        }

        void DumpToLog();
    };
}


#endif //EDITOR_KEYPRESS_H

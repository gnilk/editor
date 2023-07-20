//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_KBDBASEMONITOR_H
#define EDITOR_KBDBASEMONITOR_H

#include "Core/Keyboard.h"
#include <functional>

namespace gedit {
    class KeyboardBaseMonitor {
    public:
        using OnKeyPressDelegate = std::function<void(Keyboard::HWKeyEvent &event)>;
    public:
        KeyboardBaseMonitor() = default;
        virtual ~KeyboardBaseMonitor() = default;

        // This allows the base to be used without specialization - the base never fails to start (it doesn't do anything)
        // Removes a few #ifdef's in the code path...
        virtual bool Start() { return true; }

        virtual bool IsModifierPressed(Keyboard::kModifierKeys ctrlKey) { return false; }
        virtual uint8_t GetModifiersCurrentlyPressed() const { return 0; }

        void SetOnKeyPressDelegate(OnKeyPressDelegate newKeyPressHandler) {
            cbKeyPress = newKeyPressHandler;
        }
    protected:
        OnKeyPressDelegate cbKeyPress = nullptr;
    };
}
#endif //EDITOR_KBDBASEMONITOR_H

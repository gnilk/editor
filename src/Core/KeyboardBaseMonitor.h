//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_KBDBASEMONITOR_H
#define EDITOR_KBDBASEMONITOR_H

#include "Core/KeyCodes.h"
#include <functional>

// KeyboardBaseMonitor
class KeyboardBaseMonitor {
public:
    using OnKeyPressDelegate = std::function<void(Keyboard::HWKeyEvent &event)>;
public:
    KeyboardBaseMonitor() = default;
    virtual ~KeyboardBaseMonitor() = default;

    virtual bool Start() { return false; }
    virtual bool IsModifierPressed(Keyboard::kModifierKeys ctrlKey) { return false; }
    virtual uint8_t GetModifiersCurrentlyPressed() const { return 0; }

    void SetOnKeyPressDelegate(OnKeyPressDelegate newKeyPressHandler) {
        cbKeyPress = newKeyPressHandler;
    }
protected:
    OnKeyPressDelegate cbKeyPress = nullptr;
};

#endif //EDITOR_KBDBASEMONITOR_H

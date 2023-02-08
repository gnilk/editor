//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_MACOSKEYBOARDMONITOR_H
#define EDITOR_MACOSKEYBOARDMONITOR_H

#include <map>
#include "Core/KeyCodes.h"
#include "Core/KeyboardBaseMonitor.h"

//
// Note: Events emitted through the event listener (see: base class)
// WILL BE ON ANOTHER THREAD!!!!!!!!!
//
class MacOSKeyboardMonitor : public KeyboardBaseMonitor {
public:
    MacOSKeyboardMonitor() = default;
    virtual ~MacOSKeyboardMonitor() = default;

    bool Start() override;
    void SetDebug(bool newDebug) {
        bDebug = newDebug;
    }
    // Emit events for only modifiers - i.e. when the incoming scan code is a modifier
    // By default such events will be suppressed and instead part of the event for a 'regular' key...
    // i.e. CTRL down, will not generate an event. but CTRL+LeftArrow will generate 'LeftArrow' + modifiers properly set
    void SetEventNotificationsForModifiers(bool bEnable) {
        bEmitEventsForModifiers = bEnable;
    }
    void SetEventNotificationsForKeyUpDown(bool notifyKeyDown, bool notifyKeyUp) {
        bNotifyKeyDown = notifyKeyDown;
        bNotifyKeyUp = notifyKeyUp;
    }
    bool IsModifierPressed(Keyboard::kModifierKeys ctrlKey) override;
    uint8_t GetModifiersCurrentlyPressed() const override;
public:
    void OnKeyEvent(uint32_t scancode, long pressed, int32_t pid);
protected:
    Keyboard::kKeyCode TranslateScanCodeToKeyCode(uint32_t scancode);
    uint8_t TranslateScanCodeToASCII(uint32_t scancode);
private:
    bool bDebug = false;
    bool bEmitEventsForModifiers = false;
    bool bNotifyKeyDown = true;
    bool bNotifyKeyUp = false;
    std::map<Keyboard::kModifierKeys, bool> modifierKeyStatus;
};

#endif //EDITOR_MACOSKEYBOARDMONITOR_H

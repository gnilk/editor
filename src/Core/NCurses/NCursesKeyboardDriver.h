//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_NCURSESKEYBOARDDRIVER_H
#define EDITOR_NCURSESKEYBOARDDRIVER_H

#include <ncurses.h>

#include "Core/KeyboardDriverBase.h"
#include "Core/macOS/MacOSKeyboardMonitor.h"
#include "Core/SafeQueue.h"

namespace gedit {
    class NCursesKeyboardDriverNew : public KeyboardDriverBase {
    public:

        struct KeyPress {
            bool isKeyValid = false;
            bool isHwEventValid = false;
            Keyboard::HWKeyEvent hwEvent;
            uint8_t modifiers;
            int key;

            // Human readables..  NOTE: we don't support unicode..  I'm old-skool...
            bool IsHumanReadable() {
                if (isKeyValid) {
                    if ((key > 31) && (key < 127)) {
                        return true;
                    }
                }
                return false;
            }
        };
    public:
        NCursesKeyboardDriverNew() = default;
        void Begin(MacOSKeyboardMonitor *monitor) {
            ptrKeyboardMonitor = monitor;

            ptrKeyboardMonitor->SetOnKeyPressDelegate([this](Keyboard::HWKeyEvent &event) {
                kbdEvents.push(event);
            });
        }
        // since we are monitoring _all_ keys in the system
        // the stdin for PID will run out of sync...
        KeyPress GetKeyPress() {
            KeyPress keyPress;

            auto ch = getch();
            if (!kbdEvents.empty()) {
                while ((ch = getch()) == ERR) {
                    // FIXME: timeout handling
                }
                keyPress.isHwEventValid = true;
                // Remove and assign the last to match with the getch
                while (!kbdEvents.empty()) {
                    keyPress.hwEvent = kbdEvents.pop();
                }

            }
            keyPress.isKeyValid = (ch == ERR) ? false : true;
            keyPress.modifiers = ptrKeyboardMonitor->GetModifiersCurrentlyPressed();
            keyPress.key = ch;
            return keyPress;

        }
    private:
        MacOSKeyboardMonitor *ptrKeyboardMonitor;
        SafeQueue <Keyboard::HWKeyEvent> kbdEvents;

    };
}


//
// OLD - remove me!
//
class NCursesKeyboardDriver : public KeyboardDriverBase {
public:
    bool Initialize() override;
    KeyPress GetCh() const override;
    KeyboardBaseMonitor *Monitor() override {
        return &kbdMonitor;
    }
protected:
    KeyPress Translate(int ch) const;
private:
    MacOSKeyboardMonitor kbdMonitor;
    SafeQueue<Keyboard::HWKeyEvent> keyEventQueue;
};

#endif //EDITOR_NCURSESKEYBOARDDRIVER_H

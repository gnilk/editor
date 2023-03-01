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
    class NCursesKeyboardDriver : public KeyboardDriverBase {
    public:
        NCursesKeyboardDriver() = default;
        void Begin(MacOSKeyboardMonitor *monitor) {
            ptrKeyboardMonitor = monitor;

            ptrKeyboardMonitor->SetOnKeyPressDelegate([this](Keyboard::HWKeyEvent &event) {
                kbdEvents.push(event);
            });
        }
        // since we are monitoring _all_ keys in the system
        // the stdin for PID will run out of sync...
        KeyPress GetKeyPress();
    private:
        int TranslateNCurseKey(int ch);
    private:
        MacOSKeyboardMonitor *ptrKeyboardMonitor;
        SafeQueue <Keyboard::HWKeyEvent> kbdEvents;

    };
}

#endif //EDITOR_NCURSESKEYBOARDDRIVER_H

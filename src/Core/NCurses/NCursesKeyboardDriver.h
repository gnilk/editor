//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_NCURSESKEYBOARDDRIVER_H
#define EDITOR_NCURSESKEYBOARDDRIVER_H

#include "Core/KeyboardDriverBase.h"
#include "Core/macOS/MacOSKeyboardMonitor.h"
#include "Core/SafeQueue.h"


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

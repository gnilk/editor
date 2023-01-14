//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_KBDBASEMONITOR_H
#define EDITOR_KBDBASEMONITOR_H

#include "Core/KeyCodes.h"

// KeyboardBaseMonitor
class KeyboardBaseMonitor {
public:
    KeyboardBaseMonitor() = default;
    virtual ~KeyboardBaseMonitor() = default;

    virtual bool Start() { return false; }
    virtual bool IsPressed(kStdControlKeys ctrlKey) { return false; }
    virtual int32_t GetSpecialCurrentlyPressed() { return 0; }
};

#endif //EDITOR_KBDBASEMONITOR_H

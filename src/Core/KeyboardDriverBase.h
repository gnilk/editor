//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_KEYBOARDDRIVERBASE_H
#define EDITOR_KEYBOARDDRIVERBASE_H

#include <cstdint>
#include "Core/KeyPress.h"
#include "KeyboardBaseMonitor.h"


class KeyboardDriverBase {
public:
    virtual bool Initialize() { return false; };
    virtual KeyPress GetCh() const;
    void SetDebugMode(bool enable) {
        debugMode = enable;
    }
    virtual KeyboardBaseMonitor *Monitor() { return nullptr; }

protected:
    bool debugMode = false;
};

#endif //EDITOR_KEYBOARDDRIVERBASE_H

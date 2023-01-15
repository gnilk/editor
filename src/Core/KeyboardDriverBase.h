//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_KEYBOARDDRIVERBASE_H
#define EDITOR_KEYBOARDDRIVERBASE_H

#include <cstdint>
#include "Core/KeyPress.h"


class KeyboardDriverBase {
public:
    virtual bool Initialize() { return false; };
    virtual KeyPress GetCh();
    void SetDebugMode(bool enable) {
        debugMode = enable;
    }
protected:
    bool debugMode = false;
};

#endif //EDITOR_KEYBOARDDRIVERBASE_H

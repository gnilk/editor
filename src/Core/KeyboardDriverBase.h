//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_KEYBOARDDRIVERBASE_H
#define EDITOR_KEYBOARDDRIVERBASE_H

#include <cstdint>
#include "Core/KeyPress.h"
#include "KeyboardBaseMonitor.h"
#include <memory>

namespace gedit {
    class KeyboardDriverBase {
    public:
        using Ref = std::shared_ptr<KeyboardDriverBase>;
    public:
        virtual bool Initialize() { return false; };
        virtual void Close() {}

        virtual KeyPress GetKeyPress() { return {}; }
        void SetDebugMode(bool enable) {
            debugMode = enable;
        }
        virtual KeyboardBaseMonitor *Monitor() { return nullptr; }

        virtual void TempFuncReleaseKeyPressFunc() {}

    protected:
        bool debugMode = false;
    };
}

#endif //EDITOR_KEYBOARDDRIVERBASE_H

//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_MACOSKEYBOARDMONITOR_H
#define EDITOR_MACOSKEYBOARDMONITOR_H

#include <map>
#include "Core/KeyCodes.h"
#include "Core/KeyboardBaseMonitor.h"

class MacOSKeyboardMonitor : public KeyboardBaseMonitor {
public:
    MacOSKeyboardMonitor() = default;
    virtual ~MacOSKeyboardMonitor() = default;

    bool Start() override;
    bool IsPressed(kStdControlKeys ctrlKey) override;
    int32_t GetSpecialCurrentlyPressed() override;
public:
    void OnKeyEvent(uint32_t scancode, long pressed, int32_t pid);
private:
    std::map<kStdControlKeys, bool> keyPressStatus;
};

#endif //EDITOR_MACOSKEYBOARDMONITOR_H

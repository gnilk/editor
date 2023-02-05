//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_RUNTIMECONFIG_H
#define EDITOR_RUNTIMECONFIG_H

#include "Core/ScreenBase.h"
#include "Core/KeyboardDriverBase.h"
#include "Core/ModeBase.h"

class RuntimeConfig {
public:
    static RuntimeConfig &Instance();

    void SetKeyboard(KeyboardDriverBase &kbd) {
        keyboard = &kbd;
    }
    void SetScreen(ScreenBase &scr) {
        screen = &scr;
    }
    void SetOutputConsole(IOutputConsole *newOutputConsole) {
        outputConsole = newOutputConsole;
    }

    KeyboardDriverBase *Keyboard() {
        return keyboard;
    }
    ScreenBase *Screen() {
        return screen;
    }
    IOutputConsole *OutputConsole() {
        return outputConsole;
    }


private:
    RuntimeConfig() = default;
private:
    KeyboardDriverBase *keyboard = nullptr;
    ScreenBase *screen = nullptr;
    IOutputConsole *outputConsole = nullptr;

};

#endif //EDITOR_RUNTIMECONFIG_H

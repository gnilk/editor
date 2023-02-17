//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_RUNTIMECONFIG_H
#define EDITOR_RUNTIMECONFIG_H

#include "Core/ScreenBase.h"
#include "Core/KeyboardDriverBase.h"
#include "Core/ModeBase.h"
#include "Core/Views/ViewBase.h"

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
    void SetRootView(gedit::ViewBase *newRootView) {
        view = newRootView;
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

    gedit::ViewBase *View() {
        return view;
    }


private:
    RuntimeConfig() = default;
private:
    KeyboardDriverBase *keyboard = nullptr;
    ScreenBase *screen = nullptr;
    gedit::ViewBase *view = nullptr;
    IOutputConsole *outputConsole = nullptr;

};

#endif //EDITOR_RUNTIMECONFIG_H

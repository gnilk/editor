//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_RUNTIMECONFIG_H
#define EDITOR_RUNTIMECONFIG_H

#include "Core/ScreenBase.h"
#include "Core/KeyboardDriverBase.h"
#include "Core/Views/ViewBase.h"
#include "Core/EditorModel.h"

namespace gedit {
    class IOutputConsole {
    public:
        virtual void WriteLine(const std::string &str) = 0;
    };
    // Should have active buffer
    class RuntimeConfig {
    public:
        static RuntimeConfig &Instance();

        void SetActiveEditorModel(EditorModel::Ref newActiveEditorModel) {
            activeEditorModel = newActiveEditorModel;
        }
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
        void SetWindow(WindowBase *newWindow) {
            window = newWindow;
        }

        EditorModel::Ref ActiveEditorModel() {
            return activeEditorModel;
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

        WindowBase *Window() {
            return window;
        }

        ViewBase *View() {
            return view;
        }


    private:
        RuntimeConfig() = default;
    private:
        EditorModel::Ref activeEditorModel;
        KeyboardDriverBase *keyboard = nullptr;
        ScreenBase *screen = nullptr;
        ViewBase *view = nullptr;
        WindowBase *window = nullptr;
        IOutputConsole *outputConsole = nullptr;

    };
}

#endif //EDITOR_RUNTIMECONFIG_H

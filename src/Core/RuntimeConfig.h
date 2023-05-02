//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_RUNTIMECONFIG_H
#define EDITOR_RUNTIMECONFIG_H

#include "Core/ScreenBase.h"
#include "Core/KeyboardDriverBase.h"
#include "Core/Views/ViewBase.h"
#include "Core/EditorModel.h"
#include "Core/AssetLoaderBase.h"

#include <thread>

namespace gedit {
    class IOutputConsole {
    public:
        virtual void WriteLine(const std::string &str) = 0;
    };
    // Should have active buffer
    class RuntimeConfig {
    public:
        static RuntimeConfig &Instance();

        void SetMainThreadID() {
            mainThreadId = std::this_thread::get_id();
        }

        void SetActiveEditorModel(EditorModel::Ref newActiveEditorModel) {
            if (activeEditorModel != nullptr) {
                activeEditorModel->SetActive(false);
            }
            activeEditorModel = newActiveEditorModel;
            activeEditorModel->SetActive(true);
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
            rootView = newRootView;
        }
        void SetWindow(WindowBase *newWindow) {
            window = newWindow;
        }

        // Compile time..
        AssetLoaderBase &GetAssetLoader() {
            return assetLoader;
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

        bool IsRootView(ViewBase *other) {
            return (other == rootView);
        }
        ViewBase &GetRootView() {
            return *rootView;
        }

        std::thread::id MainThread() {
            return mainThreadId;
        }


    private:
        RuntimeConfig() = default;
    private:
        EditorModel::Ref activeEditorModel;
        // These should all be ref's...
        KeyboardDriverBase *keyboard = nullptr;
        ScreenBase *screen = nullptr;
        ViewBase *rootView = nullptr;
        WindowBase *window = nullptr;
        IOutputConsole *outputConsole = nullptr;
        std::thread::id mainThreadId;

        AssetLoaderBase assetLoader;

    };
}

#endif //EDITOR_RUNTIMECONFIG_H

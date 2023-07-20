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
#include "Core/Plugins/PluginCommand.h"

#include <map>
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

        void SetKeyboard(KeyboardDriverBase::Ref kbd) {
            keyboard = kbd;
        }
        void SetScreen(ScreenBase::Ref scr) {
            screen = scr;
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

        KeyboardDriverBase::Ref GetKeyboard() {
            return keyboard;
        }
        ScreenBase::Ref GetScreen() {
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
        bool HasRootView() {
            return (rootView != nullptr);
        }

        std::thread::id MainThread() {
            return mainThreadId;
        }

        void SetQuickCmdView(ViewBase *view) {
            quickModeView = view;
        }
        ViewBase *GetQuickCmdView() {
            return quickModeView;
        }



        // Not sure this should be here - perhaps rather in the editor...
        void RegisterPluginCommand(const PluginCommand::Ref pluginCommand);
        bool HasPluginCommand(const std::string &name);
        PluginCommand::Ref GetPluginCommand(const std::string &name);
        std::vector<PluginCommand::Ref> GetPluginCommands();

    private:
        RuntimeConfig() = default;
    private:
        // These should all be ref's...
        KeyboardDriverBase::Ref keyboard = nullptr;
        ScreenBase::Ref screen = nullptr;
        ViewBase *rootView = nullptr;
        WindowBase *window = nullptr;
        IOutputConsole *outputConsole = nullptr;
        std::thread::id mainThreadId;
        AssetLoaderBase assetLoader;

        std::map<std::string, const PluginCommand::Ref> pluginCommands;
        ViewBase *quickModeView = nullptr;

    };
}

#endif //EDITOR_RUNTIMECONFIG_H

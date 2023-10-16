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
#include "Core/FolderMonitor.h"

#ifdef GEDIT_MACOS
#include "Core/macOS/MacOSFolderMonitor.h"
#elif defined(GEDIT_LINUX)
#include "Core/Linux/LinuxFolderMonitor.h"
#endif


#include <map>
#include <thread>
#include "Core/WindowLocation.h"
#include "Core/TimerController.h"

namespace gedit {
    class IOutputConsole {
    public:
        virtual void WriteLine(const std::u32string &str) = 0;
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
        WindowLocation &GetWindowLocation() {
            return windowLocation;
        }

        // Compile time..
        AssetLoaderBase &GetAssetLoader() {
            return assetLoader;
        }

        KeyboardDriverBase::Ref GetKeyboard() {
            return keyboard;
        }

        // Start and return the timer controller...
        TimerController &GetTimerController() {
            timerController.Start();
            return timerController;
        }

        FolderMonitor &GetFolderMonitor();

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

        template<class T>
        T *GetRootViewAs() {
           return static_cast<T *>(rootView);
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
        bool HasPluginCommand(const std::u32string &name);
        PluginCommand::Ref GetPluginCommand(const std::string &name);
        PluginCommand::Ref GetPluginCommand(const std::u32string &name);
        std::vector<PluginCommand::Ref> GetPluginCommands();

    private:
        RuntimeConfig() = default;
    private:
        // These should all be ref's...
#ifdef GEDIT_MACOS
        MacOSFolderMonitor folderMonitor;
#elif defined(GEDIT_LINUX)
        LinuxFolderMonitor folderMonitor;
#endif
        KeyboardDriverBase::Ref keyboard = nullptr;
        ScreenBase::Ref screen = nullptr;
        ViewBase *rootView = nullptr;
        WindowBase *window = nullptr;
        IOutputConsole *outputConsole = nullptr;
        std::thread::id mainThreadId;
        AssetLoaderBase assetLoader;
        WindowLocation windowLocation;
        TimerController timerController;

        std::map<std::string, const PluginCommand::Ref> pluginCommands;
        ViewBase *quickModeView = nullptr;

    };
}

#endif //EDITOR_RUNTIMECONFIG_H

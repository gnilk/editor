//
// AI-3 (docs/ui-refactor.md §8): the toolkit's only "ambient" dependency - a slim UI-owned context
// carrying just the screen + root-view + quick-cmd-view slots, instead of reaching into the app's
// RuntimeConfig god-object (which also drags in Document/Plugins/FolderMonitor). The app feeds this
// once at startup (main.cpp + Editor::Setup*) alongside the existing RuntimeConfig wiring - both are
// populated from the same call sites for now (no single source of truth yet; see docs/ui-refactor.md).
//

#ifndef GEDIT_UIHOST_H
#define GEDIT_UIHOST_H

#include <functional>
#include "Core/UI/Graphics/ScreenBase.h"
#include "Core/NamedColors.h"

namespace gedit {

    class ViewBase;

    class UIHost {
    public:
        static UIHost &Instance();

        void SetScreen(ScreenBase::Ref newScreen) {
            screen = newScreen;
        }
        ScreenBase::Ref GetScreen() const {
            return screen;
        }

        void SetRootView(ViewBase *newRootView) {
            rootView = newRootView;
        }
        ViewBase &GetRootView() const {
            return *rootView;
        }
        bool HasRootView() const {
            return rootView != nullptr;
        }
        bool IsRootView(const ViewBase *other) const {
            return other == rootView;
        }

        // The overlay view (e.g. the quick-command bar) that owns the OS cursor while active. The
        // toolkit treats this as an opaque alternate view; it has no notion of *why* it's active (that
        // stays an editor concept - see ViewBase::SetWindowCursor).
        void SetQuickCmdView(ViewBase *view) {
            quickCmdView = view;
        }
        ViewBase *GetQuickCmdView() const {
            return quickCmdView;
        }

        // AI-6 (docs/ui-refactor.md §8): whether quick-command mode is active right now - a plain
        // bool mirroring quickCmdView's "opaque overlay" treatment above. The app pushes this in at
        // the two real Editor::state transitions (Editor.cpp); the toolkit only ever queries it (e.g.
        // ViewBase::SetWindowCursor redirects to quickCmdView while active).
        void SetQuickCommandActive(bool active) {
            quickCommandActive = active;
        }
        bool IsQuickCommandActive() const {
            return quickCommandActive;
        }

        // AI-6: "should switching the active top view auto-leave quick-command mode" is app policy
        // (reads Config's quickmode.leave_when_switching_view + calls Editor::LeaveCommandMode) - not
        // a UI concern. RootView::LeaveQuickCommand() just invokes this if set; the app wires the real
        // behavior once at startup (mirrors ViewBase::SetLayoutChangedHandler, AI-5).
        void SetOnLeaveQuickCommand(std::function<void()> hook) {
            onLeaveQuickCommand = std::move(hook);
        }
        void NotifyLeaveQuickCommand() const {
            if (onLeaveQuickCommand) {
                onLeaveQuickCommand();
            }
        }

        // AI-6: copies of the active theme's color tables, pushed in by the app whenever the theme
        // (re)loads (Editor::ConfigureTheme, ThemeAPI::Reload). Copies, not a Theme::Ref - Theme
        // extends ConfigNode (an app/config concept) and Theme::Reload() clears+rebuilds colorConfig,
        // so holding a live reference into it would dangle across a reload.
        void SetUIColors(const NamedColors &colors) {
            uiColors = colors;
        }
        const NamedColors &GetUIColors() const {
            return uiColors;
        }
        void SetGlobalColors(const NamedColors &colors) {
            globalColors = colors;
        }
        const NamedColors &GetGlobalColors() const {
            return globalColors;
        }

    private:
        UIHost() = default;

        ScreenBase::Ref screen = nullptr;
        ViewBase *rootView = nullptr;
        ViewBase *quickCmdView = nullptr;
        bool quickCommandActive = false;
        std::function<void()> onLeaveQuickCommand = nullptr;
        NamedColors uiColors;
        NamedColors globalColors;
    };

}

#endif //GEDIT_UIHOST_H

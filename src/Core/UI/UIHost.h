//
// AI-3 (docs/ui-refactor.md §8): the toolkit's only "ambient" dependency - a slim UI-owned context
// carrying just the screen + root-view + quick-cmd-view slots, instead of reaching into the app's
// RuntimeConfig god-object (which also drags in Document/Plugins/FolderMonitor). The app feeds this
// once at startup (main.cpp + Editor::Setup*) alongside the existing RuntimeConfig wiring - both are
// populated from the same call sites for now (no single source of truth yet; see docs/ui-refactor.md).
//

#ifndef GEDIT_UIHOST_H
#define GEDIT_UIHOST_H

#include "Core/UI/Graphics/ScreenBase.h"

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

    private:
        UIHost() = default;

        ScreenBase::Ref screen = nullptr;
        ViewBase *rootView = nullptr;
        ViewBase *quickCmdView = nullptr;
    };

}

#endif //GEDIT_UIHOST_H

//
// Created by gnilk on 24.02.23.
//

#ifndef GEDIT_ROOTVIEW_H
#define GEDIT_ROOTVIEW_H

#include <algorithm>
#include <vector>

#include "logger.h"

#include "Core/UI/UIHost.h"
#include "Core/MouseEvent.h"
#include "ViewBase.h"
#include "ModalView.h"

namespace gedit {
    // The root view should be the first view and cover the full screen...
    class RootView : public ViewBase {
    private:
        struct TopViewInstance {
            std::string name;
            ViewBase::Ref view;
        };
    public:
        RootView() {
        }
        void InitView() override {
            auto screen = UIHost::Instance().GetScreen();
            viewRect = screen->Dimensions();
            window = screen->CreateWindow(viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);
        }
        void ReInitView() override {
            auto screen = UIHost::Instance().GetScreen();
            viewRect = screen->Dimensions();
            window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);
            // Modals aren't subviews - re-centre them on the new screen
            for (auto &modal : modals) {
                modal->Resize();
            }
        }

        virtual void Draw() override {
            auto logger = gnilk::Logger::GetLogger("RootView");
            logger->Debug("==== Begin Draw Cycle ====");
            // A modal closed from outside an input dispatch (a posted message) goes here
            ReapClosedModals();
            ViewBase::Draw();
            // Bottom to top, a nested modal sits on its parent
            for (auto &modal : modals) {
                modal->Draw();
            }
            logger->Debug("==== End Draw Cycle ====");
        }

        //
        // Modal host (docs/modal_fixes.md §3.1). Non-blocking: the modal is drawn on top of the tree and
        // gets all input until it closes. A modal may show another one (a picker -> a confirm).
        //
        void ShowModal(ModalView::Ptr modal) {
            auto wasModalActive = HasModal();
            if (wasModalActive) {
                TopModal()->SetActive(false);
            }
            SetOverlayParent(modal.get());
            modal->Initialize();
            modal->SetActive(true);
            modals.push_back(std::move(modal));
            InvalidateAll();
            if (!wasModalActive) {
                UIHost::Instance().NotifyModalActive(true);
            }
        }

        bool HasModal() const {
            return !modals.empty();
        }

        ModalView *TopModal() {
            if (modals.empty()) {
                return nullptr;
            }
            return modals.back().get();
        }

        // Layout session (docs/session-cache.md §4.1) — the focused top-view is a RootView concept, so
        // RootView owns persisting it. Walked in by ViewBase::CollectLayout / ApplyLayout.
        void ToSession(ILayoutSink &sink) override {
            auto name = GetTopViewName();
            if (name.has_value()) {
                sink.PutFocusedTopView(*name);
            }
        }
        void FromSession(const ILayoutSink &sink) override {
            std::string name;
            if (sink.GetFocusedTopView(name)) {
                SetActiveTopViewByName(name);
            }
        }

        void AddTopView(ViewBase *view, const std::string &name) {

            // Let's do it like this for now...
            auto ref = std::shared_ptr<ViewBase>(view, [](ViewBase *ptr){
            });

            TopViewInstance topView = {name, ref};
            topViews.push_back(topView);
            if (idxCurrentTopView == -1) {
                idxCurrentTopView = 0;
                if (IsInitialized()) {
                    TopView()->SetActive(true);
                }
            }
        }

        const std::vector<std::string> GetTopViews() {
            std::vector<std::string> viewNames;
            for(auto &t : topViews) {
                viewNames.push_back(t.name);
            }
            return viewNames;
        }

        bool SetActiveTopViewByName(const std::string &name) {
            // check if the current view is our view - apparently we don't like switching to the same view..
            auto currentName = GetTopViewName();
            if (currentName.has_value() && name == currentName) {
                return true;
            }

            auto currentView = TopView();

            for(size_t i=0;i<topViews.size();i++) {
                if (topViews[i].name == name) {
                    idxCurrentTopView = (int)i;
                    LeaveQuickCommand();
                    TopView()->SetActive(true);
                    currentView->SetActive(false);
                    return true;
                }
            }
            return false;
        }

        ViewBase::Ref GetTopViewByName(const std::string &name) {
            for(auto &t : topViews) {
                if (t.name == name) {
                    return t.view;
                }
            }
            return nullptr;
        }

        ViewBase::Ref TopView() {
            if (idxCurrentTopView == -1) {
                return nullptr;
            }
            return topViews[idxCurrentTopView].view;
        }

        std::optional<std::string> GetTopViewName() {
            if (idxCurrentTopView == -1) {
                return {};
            }
            return {topViews[idxCurrentTopView].name};
        }

        bool OnAction(const EditorAction &kpAction) override {
            // The top modal takes everything - an action it doesn't handle goes nowhere else
            if (HasModal()) {
                TopModal()->OnAction(kpAction);
                ReapClosedModals();
                return true;
            }

            // Dispatch to top-view...
            if ((TopView() != nullptr) && TopView()->OnAction(kpAction)) {
                return true;
            }

            switch(kpAction.uiAction) {
                case kUIAction::kUIActionCycleActiveView :
                    OnCycleActiveView();
                    return true;
                case kUIAction::kUIActionCycleActiveViewNext :
                    OnCycleActiveViewNext();
                    return true;
                case kUIAction::kUIActionCycleActiveViewPrev :
                    OnCycleActiveViewPrev();
                    return true;
                default:
                    break;
            }

            if (kpAction.action == kAction::kActionEnterCommandMode) {
                OnEnterCommandMode();
            }
            return false;
        }

        void HandleKeyPress(const KeyPress &keyPress) override {
            if (HasModal()) {
                TopModal()->HandleKeyPress(keyPress);
                ReapClosedModals();
                return;
            }
            if (TopView() == nullptr) {
                return;
            }
            // Note: we don't call 'OnKeyPress' here as it would result in a infinte recursive loop
            TopView()->HandleKeyPress(keyPress);
        }

        // Mouse is spatially-routed (see ViewBase::OnMouseEvent), so it gets its own dispatch path
        // separate from HandleKeyPress: (1) a modal restricts the hit-test to its own subtree (and
        // swallows the event if the click misses it), (2) hit-test the real subview tree (the same
        // tree Draw() walks - topViews are a parallel name->ViewBase* registry over the same
        // instances, see AddTopView), (3) activate the enclosing top view (focus-follows-click), (4)
        // call OnMouseEvent on the hit view, bubbling to parentView while it returns false.
        bool DispatchMouse(const MouseEvent &mouseEvent) {
            ViewBase *hit = nullptr;
            if (HasModal()) {
                auto modal = TopModal();
                hit = modal->HitTest(mouseEvent.x, mouseEvent.y);
                if (hit == nullptr) {
                    return true; // swallow - modal owns all input while active
                }
                // Stop at the modal - its overlay parent (us) is not part of the dialog
                for (auto view = hit; view != nullptr; view = view->GetParentView()) {
                    if (view->OnMouseEvent(mouseEvent) || (view == modal)) {
                        break;
                    }
                }
                ReapClosedModals();
                return true;
            } else {
                hit = HitTest(mouseEvent.x, mouseEvent.y);
                if (hit == nullptr) {
                    return false;
                }
                // Focus-follows-click activates on the PRESS (and wheel), never on the trailing RELEASE.
                // A release is the tail of a gesture already focused on its press - re-activating the
                // hit view's top view here would clobber any focus change the press handler just made
                // (e.g. WorkspaceView double-click opens a file and switches to the editor view; the
                // release that follows must not yank focus back to the workspace panel).
                if (mouseEvent.kind != MouseEvent::kMouseEventKind_Release) {
                    auto topViewName = FindEnclosingTopViewName(hit);
                    if (topViewName.has_value()) {
                        // A click is an explicit "focus this view" gesture, so it must leave
                        // quick-command mode even when the clicked view is ALREADY the active top
                        // view - SetActiveTopViewByName short-circuits the same-view case and would
                        // otherwise skip the leave, stranding the user in quick-command mode
                        // (open-bug 13). The leave policy/flag (quickmode.leave_when_switching_view)
                        // lives app-side behind this notification; a second leave from inside
                        // SetActiveTopViewByName (the cross-view case) is a harmless no-op.
                        LeaveQuickCommand();
                        SetActiveTopViewByName(*topViewName);
                    }
                }
            }

            for (auto view = hit; view != nullptr; view = view->GetParentView()) {
                if (view->OnMouseEvent(mouseEvent)) {
                    return true;
                }
            }
            return false;
        }

    protected:
        void OnViewInitialized() override {
            ViewBase::OnViewInitialized();
            if (TopView() != nullptr) {
                TopView()->SetActive(true);
            }
        }

        // This should probably go away..
        void OnCycleActiveView() {
            auto currentView = TopView();
            idxCurrentTopView = (idxCurrentTopView+1) % topViews.size();
            LeaveQuickCommand();
            TopView()->SetActive(true);
            currentView->SetActive(false);
        }
        void OnCycleActiveViewNext() {
            auto currentView = TopView();
            idxCurrentTopView = (idxCurrentTopView+1) % topViews.size();
            LeaveQuickCommand();
            TopView()->SetActive(true);
            currentView->SetActive(false);
        }
        void OnCycleActiveViewPrev() {
            auto currentView = TopView();
            idxCurrentTopView = (topViews.size() + (idxCurrentTopView-1)) % topViews.size();
            LeaveQuickCommand();
            TopView()->SetActive(true);
            currentView->SetActive(false);
        }
        void OnEnterCommandMode() {
            // need to fix this...
            auto logger = gnilk::Logger::GetLogger("RootView");
            logger->Debug("Should enter command view!");
        }

    protected:
        // Pop every modal marked closed, then call its OnClosed - never from inside the modal's own
        // dispatch. A callback may show a new modal, so look again after each one.
        void ReapClosedModals() {
            for (;;) {
                auto it = std::find_if(modals.rbegin(), modals.rend(), [](const ModalView::Ptr &modal) {
                    return modal->IsClosing();
                });
                if (it == modals.rend()) {
                    return;
                }
                auto closed = std::move(*it);
                modals.erase(std::next(it).base());
                closed->SetActive(false);
                if (HasModal()) {
                    TopModal()->SetActive(true);
                } else {
                    UIHost::Instance().NotifyModalActive(false);
                }
                InvalidateAll();
                closed->NotifyClosed();
            }
        }

        void LeaveQuickCommand() {
            UIHost::Instance().NotifyLeaveQuickCommand();
        }

        // Walk from the hit view up through parentView until we recognize one as a registered top
        // view (pointer identity against topViews) - "focus the view I clicked" must resolve to the
        // enclosing TOP VIEW, not the leaf (e.g. a click lands in EditorView, but the registered top
        // view is the enclosing EditorViewContainer).
        std::optional<std::string> FindEnclosingTopViewName(ViewBase *view) {
            for (auto v = view; v != nullptr; v = v->GetParentView()) {
                for (auto &t : topViews) {
                    if (t.view.get() == v) {
                        return {t.name};
                    }
                }
            }
            return {};
        }
    protected:
        int idxCurrentTopView = -1;
        std::vector<TopViewInstance> topViews;
        std::vector<ModalView::Ptr> modals;     // Top of the stack (back) gets the input
    };
}

#endif //NCWIN_ROOTVIEW_H

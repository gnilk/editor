//
// Created by gnilk on 24.02.23.
//

#ifndef GEDIT_ROOTVIEW_H
#define GEDIT_ROOTVIEW_H

#include "logger.h"

#include "Core/RuntimeConfig.h"
#include "ViewBase.h"

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
            auto screen = RuntimeConfig::Instance().GetScreen();
            viewRect = screen->Dimensions();
            window = screen->CreateWindow(viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);
        }
        void ReInitView() override {
            auto screen = RuntimeConfig::Instance().GetScreen();
            viewRect = screen->Dimensions();
            window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);
        }

        virtual void Draw() override {
            auto logger = gnilk::Logger::GetLogger("RootView");
            logger->Debug("==== Begin Draw Cycle ====");
            ViewBase::Draw();
            logger->Debug("==== End Draw Cycle ====");
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
            auto currentView = TopView();
            for(int i=0;i<topViews.size();i++) {
                if (topViews[i].name == name) {
                    idxCurrentTopView = i;
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

        bool OnAction(const KeyPressAction &kpAction) override {
            // The modal takes control over everything...
            if (modal != nullptr) {
                return modal->OnAction(kpAction);
            }



            bool wasHandled = true;
            // Dispatch to top-view...
            if ((TopView() != nullptr) && TopView()->OnAction(kpAction)) {
                return true;
            }

            switch(kpAction.action) {
                case kAction::kActionEnterCommandMode :
                    OnEnterCommandMode();
                    wasHandled = false;
                    break;
                case kAction::kActionCycleActiveView :
                    OnCycleActiveView();
                    break;
                case kAction::kActionCycleActiveViewNext :
                    OnCycleActiveViewNext();
                    break;
                case kAction::kActionCycleActiveViewPrev :
                    OnCycleActiveViewPrev();
                    break;
                default:
                    wasHandled = false;
            }
            return wasHandled;
        }

        void HandleKeyPress(const KeyPress &keyPress) override {
            if (modal != nullptr) {
                modal->HandleKeyPress(keyPress);
            }
            if (TopView() == nullptr) {
                return;
            }
            // Note: we don't call 'OnKeyPress' here as it would result in a infinte recursive loop
            TopView()->HandleKeyPress(keyPress);
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
        void LeaveQuickCommand() {
            auto bAutoLeave = Config::Instance()["quickmode"].GetBool("leave_when_switching_view", true);
            if ((Editor::Instance().GetState() == Editor::State::QuickCommandState) && bAutoLeave) {
                Editor::Instance().LeaveCommandMode();
            }
        }
    protected:
        int idxCurrentTopView = -1;
        std::vector<TopViewInstance> topViews;
    };
}

#endif //NCWIN_ROOTVIEW_H

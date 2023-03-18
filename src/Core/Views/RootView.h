//
// Created by gnilk on 24.02.23.
//

#ifndef NCWIN_ROOTVIEW_H
#define NCWIN_ROOTVIEW_H

#include "Core/RuntimeConfig.h"
#include "ViewBase.h"

namespace gedit {
    // The root view should be the first view and cover the full screen...
    class RootView : public ViewBase {
    public:
        RootView() {
        }
        void InitView() override {
            auto screen = RuntimeConfig::Instance().Screen();
            viewRect = screen->Dimensions();
            window = screen->CreateWindow(viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);
        }
        void ReInitView() override {
            auto screen = RuntimeConfig::Instance().Screen();
            viewRect = screen->Dimensions();
            window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);
        }

        virtual void Draw() override {
            auto logger = gnilk::Logger::GetLogger("RootView");
            logger->Debug("==== Begin Draw Cycle ====");
            ViewBase::Draw();
            logger->Debug("==== End Draw Cycle ====");
        }

        // TEMP
        void AddTopView(ViewBase *view) {
            topViews.push_back(view);
            if (idxCurrentTopView == -1) {
                idxCurrentTopView = 0;
                if (IsInitialized()) {
                    TopView()->SetActive(true);
                }
            }
        }

        ViewBase *TopView() {
            if (idxCurrentTopView == -1) {
                return nullptr;
            }
            return topViews[idxCurrentTopView];
        }

        void HandleKeyPress(const KeyPress &keyPress) override {
            if (TopView() == nullptr) {
                return;
            }
            TopView()->HandleKeyPress(keyPress);
        }
    protected:
        void OnViewInitialized() override {
            ViewBase::OnViewInitialized();
            if (TopView() != nullptr) {
                TopView()->SetActive(true);
            }
        }
        void OnKeyPress(const KeyPress &keyPress) override {
            if (keyPress.IsSpecialKeyPressed(Keyboard::kKeyCode_Escape)) {
                auto logger = gnilk::Logger::GetLogger("RootView");
                TopView()->SetActive(false);
                idxCurrentTopView = (idxCurrentTopView+1) % topViews.size();
                TopView()->SetActive(true);

                logger->Debug("ESC pressed, cycle active view, new = %d", idxCurrentTopView);
            } else if (keyPress.IsSpecialKeyPressed(Keyboard::kKeyCode_F3)) {
                // TEST: Cycling through actively open editors
                auto idxCurrent = Editor::Instance().GetActiveModelIndex();
                auto idxNext = Editor::Instance().NextModelIndex(idxCurrent);
                if (idxCurrent != idxNext) {
                    auto nextModel = Editor::Instance().GetModelFromIndex(idxNext);
                    RuntimeConfig::Instance().SetActiveEditorModel(nextModel);
                }
                Initialize();
                InvalidateAll();

            }
        }

    protected:
        int idxCurrentTopView = -1;
        std::vector<ViewBase *> topViews;
    };
}

#endif //NCWIN_ROOTVIEW_H

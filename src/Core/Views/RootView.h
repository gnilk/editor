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
        // TEMP
        void SetTopView(ViewBase *view) {
            topView = view;
        }
        ViewBase *TopView() {
            return topView;
        }
        void HandleKeyPress(const KeyPress &keyPress) override {
            topView->HandleKeyPress(keyPress);
        }
    protected:
        ViewBase *topView;
    };
}

#endif //NCWIN_ROOTVIEW_H

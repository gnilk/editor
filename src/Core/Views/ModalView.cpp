//
// Created by gnilk on 14.04.23.
//

#include "ModalView.h"
#include "Core/RuntimeConfig.h"

using namespace gedit;

static const WindowBase::kWinDecoration deco = (WindowBase::kWinDecoration)(WindowBase::kWinDeco_Border | WindowBase::kWinDeco_DrawCaption);
        //
        //WindowBase::kWinDeco_None;

void ModalView::InitView() {
    logger = gnilk::Logger::GetLogger("ModalView");
    logger->Debug("InitView!");

    auto screen = RuntimeConfig::Instance().Screen();
    if (viewRect.IsEmpty()) {
        auto dim = screen->Dimensions();
        // Position ourselves in the middle...
        Rect newViewRect(Point(dim.Width()/4, dim.Height()/4), dim.Width()/2, dim.Height()/2);
        //Rect newViewRect(Point(26,0), dim.Width()/2, dim.Height()/2);
        viewRect = newViewRect;

    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, deco);
    window->SetCaption("ModalView");
}
void ModalView::ReInitView() {
    auto screen = RuntimeConfig::Instance().Screen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, deco);

    auto &rect = window->GetContentDC().GetRect();
}

bool ModalView::OnAction(const KeyPressAction &kpAction) {
    // Hmm - action alias???
    if (kpAction.action == kAction::kActionCycleActiveView) {
        CloseModal();
        return true;
    }
    return false;
}
void ModalView::OnKeyPress(const KeyPress &keyPress) {
    if (keyPress.isSpecialKey && (keyPress.specialKey == Keyboard::kKeyCode::kKeyCode_Escape)) {
        CloseModal();
    }
}
void ModalView::DrawViewContents() {
//    auto &dc = window->GetContentDC();
//    dc.DrawStringAt(1,1,"hello modal");
}


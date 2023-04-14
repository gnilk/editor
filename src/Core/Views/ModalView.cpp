//
// Created by gnilk on 14.04.23.
//

#include "ModalView.h"
#include "Core/RuntimeConfig.h"

using namespace gedit;

void ModalView::InitView() {
    logger = gnilk::Logger::GetLogger("EditorView");
    logger->Debug("InitView!");

    auto screen = RuntimeConfig::Instance().Screen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("ModalView");
}
void ModalView::ReInitView() {
    auto screen = RuntimeConfig::Instance().Screen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);

    auto &rect = window->GetContentDC().GetRect();
}

bool ModalView::OnAction(const KeyPressAction &kpAction) {
    // Hmm - action alias???
    if (kpAction.action == kAction::kActionCycleActiveView) {
        CloseModal();
    }

}
void ModalView::OnKeyPress(const KeyPress &keyPress) {
    if (keyPress.isSpecialKey && (keyPress.specialKey == Keyboard::kKeyCode::kKeyCode_Escape)) {
        CloseModal();
    }
}
void ModalView::DrawViewContents() {
    auto &dc = window->GetContentDC();
    dc.DrawStringAt(1,1,"hello modal");

}


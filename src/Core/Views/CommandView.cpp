//
// Created by gnilk on 15.02.23.
//

#include "Core/RuntimeConfig.h"
#include "CommandView.h"
#include "Core/Views/HSplitView.h"

using namespace gedit;

void CommandView::InitView() {
    // We own the cursor, so we need to reset it on new lines...
    logger = gnilk::Logger::GetLogger("CommandView");

    auto screen = RuntimeConfig::Instance().Screen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_Border);

    commandController.SetNewLineNotificationHandler([this]()->void {
        //logger->Debug("NewLine notified!");
        cursor.position.x = 0;
        InvalidateView();
    });
    commandController.Begin();
}

void CommandView::OnActivate(bool isActive) {
    logger->Debug("OnActive, isActive: %s", isActive?"yes":"no");
    if (!isActive) {
        // store height of view..
    } else {
        // restore height of view...
    }
}


void CommandView::OnKeyPress(const KeyPress &keyPress) {

    if (commandController.HandleKeyPress(cursor, 0, keyPress)) {
        return;
    }
    if (keyPress.IsSpecialKeyPressed(Keyboard::kKeyCode_F1)) {
        parentView->AdjustHeight(-1);
    } else if (keyPress.IsSpecialKeyPressed(Keyboard::kKeyCode_F2)) {
        parentView->AdjustHeight(+1);
    }

    // Must call base class - perhaps this is a stupid thing..
    ViewBase::OnKeyPress(keyPress);
}

void CommandView::DrawViewContents() {
    auto &dc = window->GetContentDC();

    auto &lines = commandController.Lines();

    int lOffset = 0;
    if (lines.size() > (dc.GetRect().Height())) {
        lOffset = lines.size() - (dc.GetRect().Height());
    }

    for(int i=0;i<dc.GetRect().Height();i++) {
        if ((i + lOffset) >= lines.size()) {
            break;
        }
        dc.ClearLine(i);
        dc.DrawStringAt(0,i,lines[i+lOffset]->Buffer().data());
    }
    if (lines.size() > dc.GetRect().Height()-1) {
        cursor.position.y = dc.GetRect().Height()-1;
    }

}

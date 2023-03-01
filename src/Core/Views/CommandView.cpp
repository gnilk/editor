//
// Created by gnilk on 15.02.23.
//

#include "CommandView.h"

using namespace gedit;

void CommandView::Begin() {
    ViewBase::Begin();
    // We own the cursor, so we need to reset it on new lines...
    logger = gnilk::Logger::GetLogger("CommandView");
    commandController.SetNewLineNotificationHandler([this]()->void {
        logger->Debug("NewLine notified!");
       cursor.position.x = 0;
    });
    commandController.Begin();
}

void CommandView::OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) {

    commandController.HandleKeyPress(cursor, 0, keyPress);
    // Must call base class - perhaps this is a stupid thing..
    ViewBase::OnKeyPress(keyPress);
}

void CommandView::DrawViewContents() {
    auto ctx = ContentAreaDrawContext();
    ctx->Clear();

    auto &lines = commandController.Lines();

    int lOffset = 0;
    if (lines.size() > (ctx->ContextRect().Height())) {
        lOffset = lines.size() - (ctx->ContextRect().Height());
    }

    for(int i=0;i<ctx->ContextRect().Height();i++) {
        if ((i + lOffset) >= lines.size()) {
            break;
        }
        ctx->DrawStringAt(0,i,lines[i+lOffset]->Buffer().data());
    }
    if (lines.size() > ctx->ContextRect().Height()-1) {
        cursor.position.y = ctx->ContextRect().Height()-1;
    }

}

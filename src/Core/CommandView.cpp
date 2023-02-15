//
// Created by gnilk on 15.02.23.
//

#include "CommandView.h"

using namespace gedit;

void CommandView::Begin() {

}

void CommandView::OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) {

    // Must call base class - perhaps this is a stupid thing..
    ViewBase::OnKeyPress(keyPress);
}

void CommandView::DrawViewContents() {
    auto ctx = ContentAreaDrawContext();
    ctx.DrawStringAt(0,0,"Hello");
}

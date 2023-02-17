//
// Created by gnilk on 15.02.23.
//

#include "Core/RuntimeConfig.h"
#include "RootView.h"
#include "logger.h"

using namespace gedit;

void RootView::Draw() {
    if (IsInvalid()) {
        auto screen = RuntimeConfig::Instance().Screen();
        screen->Clear();
    }
    // Let's draw the rest...
    ViewBase::Draw();
}


void RootView::OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) {
    // Cycle through the top-views when escape is pressed..
    if (keyPress.key == kKey_Escape) {
        auto logger = gnilk::Logger::GetLogger("RootView");
        TopView()->SetActive(false);
        idxCurrentTopView = (idxCurrentTopView+1) % topViews.size();

        TopView()->SetActive(true);


        logger->Debug("ESC pressed, cycle active view, new = %d:%s", idxCurrentTopView,TopView()->Caption().c_str());
    }
}


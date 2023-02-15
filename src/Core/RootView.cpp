//
// Created by gnilk on 15.02.23.
//

#include "RootView.h"
#include "logger.h"

using namespace gedit;

void RootView::OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) {
    // We are root - so let's process this
    if (keyPress.key == kKey_Escape) {
        auto logger = gnilk::Logger::GetLogger("RootView");
        TopView()->SetActive(false);
        idxCurrentTopView = (idxCurrentTopView+1) % topViews.size();

        TopView()->SetActive(true);


        logger->Debug("ESC pressed, cycle active view, new = %d:%s", idxCurrentTopView,TopView()->Caption().c_str());
    }
}


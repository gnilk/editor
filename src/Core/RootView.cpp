//
// Created by gnilk on 15.02.23.
//

#include "Core/RuntimeConfig.h"
#include "RootView.h"
#include "logger.h"

using namespace gedit;
void RootView::Draw() {
    if (!IsInvalid()) {
        return;
    }
    auto screen = RuntimeConfig::Instance().Screen();
    screen->Clear();
    // Let's draw the rest...
    ViewBase::Draw();
}


void RootView::OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) {
    // We are root - so let's process this
    if (keyPress.key == kKey_Escape) {
        auto logger = gnilk::Logger::GetLogger("RootView");
        TopView()->SetActive(false);
        idxCurrentTopView = (idxCurrentTopView+1) % topViews.size();

        TopView()->SetActive(true);


        logger->Debug("ESC pressed, cycle active view, new = %d:%s", idxCurrentTopView,TopView()->Caption().c_str());
    }

    if ((keyPress.modifiers & Keyboard::kModifierKeys::kMod_LeftCommand) && (keyPress.isHwEventValid)) {
        switch (keyPress.key) {
            case 'm' :
                MaximizeView();
                break;
        }
    }

}

// FIXME: This should go to a dual-splitter view
//        Requires anchoring!!!
void RootView::MaximizeView() {
    auto topView = subviews[0];
    auto bottomView = subviews[1];
    auto &rectTop = topView->Dimensions();
    auto &rectBottom = bottomView->Dimensions();

    // Move bottom
    Point ptBottomTopLeft = rectBottom.TopLeft();
    ptBottomTopLeft.y = rectBottom.BottomRight().y - 2;
    Rect newBottomRect(ptBottomTopLeft, rectBottom.BottomRight());
    bottomView->Move(newBottomRect);

    // Move top
    Point ptTopBottomRight = rectTop.BottomRight();
    ptTopBottomRight.y = ptBottomTopLeft.y;
    Rect newTopRect(rectTop.TopLeft(), ptTopBottomRight);
    topView->Move(newTopRect);


    // This is already done, but let's be sure..
    InvalidateAll();
}


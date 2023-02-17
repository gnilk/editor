//
// Created by gnilk on 17.02.23.
//

#include "HSplitView.h"

using namespace gedit;

void HSplitView::OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) {
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
void HSplitView::MaximizeView() {
    auto topView = subviews[0];
    auto bottomView = subviews[1];
    auto &rectTop = topView->ViewRect();
    auto &rectBottom = bottomView->ViewRect();

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



//
// Created by gnilk on 17.02.23.
//

#include "HSplitView.h"

using namespace gedit;

//
// Split screen horizontally and stack items veritcally
//
HSplitView::HSplitView() : ViewBase() {
    layout.SetAnchoring(ViewLayout::kViewAnchor_VerticalStack);
}

HSplitView::HSplitView(const Rect &viewArea) : ViewBase(viewArea) {
    layout.SetAnchoring(ViewLayout::kViewAnchor_VerticalStack);
}


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

void HSplitView::ComputeInitialLayout(const Rect &rect) {
    // We occupy 100%..
    layout.SetNewRect(rect);

    int midY = (rect.BottomRight().y - rect.TopLeft().y) / 2;

    Rect upperRect(rect.TopLeft(), rect.BottomRight());
    upperRect.SetHeight(rect.Height()/2);

    Rect lowerRect(rect.TopLeft(), rect.BottomRight());
    lowerRect.SetHeight(rect.Height()/2);
    lowerRect.Move(0, midY);

    // Now compute for our initial views...
    topView->ComputeInitialLayout(upperRect);
    bottomView->ComputeInitialLayout(lowerRect);
}


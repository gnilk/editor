//
// Created by gnilk on 17.02.23.
//

#include "HSplitView.h"

#include "logger.h"
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
    auto logger = gnilk::Logger::GetLogger("HSplitView");
    if (keyPress.isKeyValid) {
        logger->Debug("OnKeyPress, key=%d (%c), modifier=0x%.2x, scan=%d (0x%.2x), translated=%d (%c)", keyPress.key, keyPress.key, keyPress.modifiers, keyPress.hwEvent.scanCode, keyPress.hwEvent.scanCode, keyPress.hwEvent.translatedScanCode, keyPress.hwEvent.translatedScanCode);
    }
    if ((keyPress.modifiers & Keyboard::kModifierKeys::kMod_LeftCommand) && (keyPress.isHwEventValid)) {
        switch (keyPress.hwEvent.translatedScanCode) {
            case 'm' :
                MaximizeView();
                break;
        }
    }
    ViewBase::OnKeyPress(keyPress);
}

// FIXME: This should go to a dual-splitter view
//        Requires anchoring!!!
void HSplitView::MaximizeView() {
    auto topView = subviews[0];
    auto bottomView = subviews[1];
    auto &rectTop = topView->ViewRect();
    auto &rectBottom = bottomView->ViewRect();

    DumpViewTree();

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

    DumpViewTree();

    // This is already done, but let's be sure..
    InvalidateAll();
}

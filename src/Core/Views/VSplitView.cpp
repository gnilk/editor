//
// Created by gnilk on 17.02.23.
//

#include "VSplitView.h"

using namespace gedit;

//
// We split the screen vertically, and stack sub-items along the horizontal axis
//
VSplitView::VSplitView() : ViewBase() {
    layout.SetAnchoring(ViewLayout::kViewAnchor_HorizontalStack);
}
VSplitView::VSplitView(const Rect &viewRect) : ViewBase(viewRect) {
    layout.SetAnchoring(ViewLayout::kViewAnchor_HorizontalStack);
}


void VSplitView::ComputeInitialLayout(const Rect &rect) {
    // We occupy 100%..
    layout.SetNewRect(rect);

    int stepX = rect.Width() / subviews.size();
    int moveX = 0;

    Point ptStart = rect.TopLeft();

    Rect nextRect(ptStart, rect.BottomRight());
    nextRect.SetWidth(stepX);
    nextRect.Move(moveX, 0);
    leftView->ComputeInitialLayout(nextRect);

    moveX += stepX;
    nextRect = Rect(ptStart, rect.BottomRight());
    nextRect.SetWidth(stepX);
    nextRect.Move(moveX, 0);
    rightView->ComputeInitialLayout(nextRect);


//    Rect rightRect(rect.TopLeft(), rect.BottomRight());
//    rightRect.SetWidth(rect.Width()/2);
//    rightRect.Move(midX, 0);
//
//    // Now compute for our initial views...
//    leftView->ComputeInitialLayout(leftRect);
//    rightView->ComputeInitialLayout(rightRect);
}

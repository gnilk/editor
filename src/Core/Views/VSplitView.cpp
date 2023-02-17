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

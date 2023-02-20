//
// Created by gnilk on 14.02.23.
//

#include "Core/Controllers/EditController.h"
#include "GutterView.h"

using namespace gedit;

GutterView::GutterView() : ViewBase() {
    // Width is important...
    layout.SetNewRect(Rect(6,0));
    SetViewAnchoring(ViewLayout::kViewAnchor_FixedWidth);
}
GutterView::GutterView(const Rect &viewArea) : ViewBase(viewArea) {
        SetViewAnchoring(ViewLayout::kViewAnchor_FixedWidth);
}


void GutterView::DrawViewContents() {

    // Can't draw if no parent.. (as it will hold the shared data
    if (ParentView() == nullptr) {
        return;
    }
    auto viewData = ParentView()->GetSharedData<EditViewSharedData>();
    if (viewData == nullptr) {
        return;
    }

    auto ctx = ViewBase::ContentAreaDrawContext();
    char str[64];
    for(int i=0;i<ctx->ContextRect().Height();i++) {
        int idxLine = i + viewData->viewTopLine;
        snprintf(str, 64, " %4d", idxLine);
        if (idxLine == viewData->idxActiveLine) {
            snprintf(str, 64, "*%4d", idxLine);
        }
        ctx->DrawStringAt(0,i,str);
    }
}

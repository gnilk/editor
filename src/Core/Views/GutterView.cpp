//
// Created by gnilk on 14.02.23.
//

#include "Core/Controllers/EditController.h"
#include "Core/RuntimeConfig.h"
#include "GutterView.h"
#include "logger.h"


using namespace gedit;

GutterView::GutterView() : ViewBase() {
    // Width is important...
    //layout.SetNewRect(Rect(6,0));
    SetWidth(10);
    //SetViewAnchoring(ViewLayout::kViewAnchor_FixedWidth);
}
GutterView::GutterView(const Rect &viewArea) : ViewBase(viewArea) {
        //SetViewAnchoring(ViewLayout::kViewAnchor_FixedWidth);
}
void GutterView::InitView() {
    auto screen = RuntimeConfig::Instance().Screen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("GutterView");


}
void GutterView::ReInitView() {
    auto screen = RuntimeConfig::Instance().Screen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
}



void GutterView::DrawViewContents() {

    // Can't draw if no parent.. (as it will hold the shared data
    if (GetParentView() == nullptr) {
        return;
    }
    auto viewData = GetParentView()->GetSharedData<EditViewSharedData>();
    if (viewData == nullptr) {
        return;
    }

    auto logger = gnilk::Logger::GetLogger("GutterView");
    logger->Debug("Active Line: %d", viewData->idxActiveLine);

    auto &ctx = window->GetContentDC();
    char str[64];
    for(int i=0;i<ctx.GetRect().Height();i++) {
        int idxLine = i + viewData->viewTopLine;

        if (idxLine >= viewData->editController.Lines().size()) {
            break;
        }

        snprintf(str, 64, " %4d", idxLine);
        if (idxLine == viewData->idxActiveLine) {
            snprintf(str, 64, "*%4d", idxLine);
        }
        ctx.DrawStringAt(0,i,str);
    }
}

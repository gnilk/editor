//
// Created by gnilk on 14.02.23.
//

#include "Core/Controllers/EditController.h"
#include "Core/RuntimeConfig.h"
#include "Core/EditorModel.h"
#include "GutterView.h"
#include "logger.h"
#include "Core/Editor.h"

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
    auto editorModel = Editor::Instance().GetActiveModel();
    if (editorModel == nullptr) {
        return;
    }

//    auto logger = gnilk::Logger::GetLogger("GutterView");
//    logger->Debug("Active Line: %d", editorModel->idxActiveLine);

    auto &dc = window->GetContentDC();

    auto uiColors = Config::Instance().GetUIColors();
    dc.SetColor(uiColors["gutter_foreground"], uiColors["gutter_background"]);

    dc.Clear();

    char str[64];
    for(int i=0;i<dc.GetRect().Height();i++) {
        size_t idxLine = i + editorModel->viewTopLine;

        if (idxLine >= editorModel->Lines().size()) {
            break;
        }

        auto line = editorModel->LineAt(idxLine);
        snprintf(str, 64, "%6zu", idxLine);
        if (idxLine == editorModel->idxActiveLine) {
            snprintf(str, 64, "%6zu  *",  idxLine);
        }
        dc.DrawStringAt(0,i,str);
    }
}

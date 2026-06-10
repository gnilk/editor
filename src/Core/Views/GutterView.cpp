//
// Created by gnilk on 14.02.23.
//

#include "Core/Controllers/EditController.h"
#include "Core/RuntimeConfig.h"
#include "Core/Document.h"
#include "GutterView.h"
#include "logger.h"
#include "Core/Editor.h"
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
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("GutterView");


}
void GutterView::ReInitView() {
    auto screen = RuntimeConfig::Instance().GetScreen();
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
    auto document = Editor::Instance().GetActiveDocument();
    if (document == nullptr) {
        return;
    }

//    auto logger = gnilk::Logger::GetLogger("GutterView");
//    logger->Debug("Active Line: %d", document->idxActiveLine);

    auto &dc = window->GetContentDC();

    auto theme = Editor::Instance().GetTheme();
    auto &uiColors = theme->GetUIColors();
    dc.SetColor(uiColors["gutter_foreground"], uiColors["gutter_background"]);

    dc.Clear();

    // The gutter is text-editor chrome: line numbers + the active-line '*', paged by lineCursor.viewTopLine.
    // In hex mode the rows are 16-byte rows (the hexdump carries its own offset column) and the caret moves
    // in byte space, so a text-line gutter can neither align with the hex caret row nor track its scroll —
    // leave it blank rather than drawing a stray, mis-placed '*'.
    if (document->GetViewMode() == DocumentViewMode::kHex) {
        return;
    }

    char str[64];
    auto &lineCursor = document->GetLineCursor();
    for(int i=0;i<dc.GetRect().Height();i++) {
        size_t idxLine = i + lineCursor.viewTopLine;

        if (idxLine >= document->Lines().size()) {
            break;
        }

        auto line = document->LineAt(idxLine);
        snprintf(str, 64, "%6zu", idxLine);
        if (idxLine == lineCursor.idxActiveLine) {
            snprintf(str, 64, "%6zu  *",  idxLine);
        }
        dc.DrawStringAt(0,i,str);
    }
}

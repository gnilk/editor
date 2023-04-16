//
// Created by gnilk on 16.04.23.
//

#include "Core/RuntimeConfig.h"
#include "VisibleView.h"

using namespace gedit;

void VisibleView::InitView() {
    auto screen = RuntimeConfig::Instance().Screen();
    if (viewRect.IsEmpty()) {
    viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("VisibleView");
}

void VisibleView::ReInitView() {
    auto screen = RuntimeConfig::Instance().Screen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
}

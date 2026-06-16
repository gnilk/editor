//
// Created by gnilk on 16.04.23.
//

#include "Core/UI/UIHost.h"
#include "VisibleView.h"

using namespace gedit;

void VisibleView::InitView() {
    auto screen = UIHost::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("VisibleView");
}

void VisibleView::ReInitView() {
    auto screen = UIHost::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
}

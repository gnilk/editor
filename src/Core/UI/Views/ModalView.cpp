//
// Created by gnilk on 14.04.23.
//

#include <algorithm>

#include "ModalView.h"
#include "Core/UI/UIHost.h"

using namespace gedit;

static const WindowBase::kWinDecoration deco = (WindowBase::kWinDecoration)(WindowBase::kWinDeco_Border | WindowBase::kWinDeco_DrawCaption);

void ModalView::InitView() {
    auto screen = UIHost::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = ComputeRect(screen->Dimensions());
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, deco);
    window->SetCaption(caption);

    if (viewPtr != nullptr) {
        auto cRect = GetContentRect();
        // Make the underlying view fill out the whole modal if needed...
        viewPtr->SetWidth(cRect.Width());
        viewPtr->SetHeight(cRect.Height());
        AddView(viewPtr);
    }
}

void ModalView::ReInitView() {
    // A resize clears the rect (unless the size is explicit) - centre again on the new screen
    auto screen = UIHost::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = ComputeRect(screen->Dimensions());
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, deco);
}

Rect ModalView::ComputeRect(const Rect &screenRect) {
    return CenterIn(screenRect, screenRect.Width()/2, screenRect.Height()/2);
}

Rect ModalView::CenterIn(const Rect &screenRect, int width, int height) {
    width = std::clamp(width, 1, std::max(1, screenRect.Width()));
    height = std::clamp(height, 1, std::max(1, screenRect.Height()));
    return Rect(Point((screenRect.Width() - width)/2, (screenRect.Height() - height)/2), width, height);
}

void ModalView::SetCaption(const std::string &newCaption) {
    caption = newCaption;
    if (window != nullptr) {
        window->SetCaption(caption);
    }
}

bool ModalView::OnAction(const EditorAction &kpAction) {
    if (kpAction.uiAction == kUIAction::kUIActionCloseModal) {
        Cancel();
        return true;
    }

    if (dispatchView != nullptr) {
        return dispatchView->OnAction(kpAction);
    } else if (viewPtr != nullptr) {
        return viewPtr->OnAction(kpAction);
    }
    return false;
}

void ModalView::Close() {
    isClosing = true;
    // The host reaps closed modals before it draws - covers a close from outside an input dispatch
    InvalidateAll();
}

void ModalView::Cancel() {
    isCancelled = true;
    Close();
}

void ModalView::DrawViewContents() {
    // Our own background - the views underneath are already drawn
    auto &dc = window->GetContentDC();
    dc.Clear();
}

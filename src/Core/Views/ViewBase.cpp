//
// Created by gnilk on 21.03.23.
//

#include "ViewBase.h"
#include "Core/Editor.h"
// Ok, need .cpp file for implementation details about MainThread
#include "Core/RuntimeConfig.h"

using namespace gedit;

void ViewBase::PostMessage(gedit::ViewBase::MessageCallback callback) {
    if (RuntimeConfig::Instance().IsRootView(this)) {
        threadMessages.push(callback);
    } else {
        RuntimeConfig::Instance().GetRootView().PostMessage(callback);
    }
}

int ViewBase::ProcessMessageQueue() {
    // We should create a copy first and the process the copy...
    int nMessages = 0;
    while(!threadMessages.empty()) {
        auto msgHandler = threadMessages.pop();
        if (!msgHandler.has_value()) {
            break;
        }
        auto handler = *msgHandler;
        handler();
        nMessages++;
    }
    return nMessages;
}

void ViewBase::SetWindowCursor(const Cursor &newCursor) {
    if (Editor::Instance().GetState() == Editor::QuickCommandState) {
        auto quickView = RuntimeConfig::Instance().GetQuickCmdView();
        quickView->SetWindowCursor(newCursor);
    } else {
        window->SetCursor(newCursor);
    }
}


void ViewBase::HandleKeyPress(const KeyPress &keyPress) {
    OnKeyPress(keyPress);
    if (keyPress.isSpecialKey) {
        //
//        if (keyPress.specialKey == Keyboard::kKeyCode_F5) {
//            auto lhandler = GetLayoutHandler();
//            auto w = lhandler->GetWidth();
//            if (w > 24) {
//                lhandler->SetWidth(w + 1);
//            }
//            RuntimeConfig::Instance().GetRootView().Initialize();
//            RuntimeConfig::Instance().GetRootView().InvalidateAll();
//        } else if (keyPress.specialKey == Keyboard::kKeyCode_F8) {
//            auto lhandler = GetLayoutHandler();
//            auto w = lhandler->GetWidth();
//            lhandler->SetWidth(w + 1);
//
//            RuntimeConfig::Instance().GetRootView().Initialize();
//            RuntimeConfig::Instance().GetRootView().InvalidateAll();
//        }
    }
}

bool ViewBase::OnAction(const KeyPressAction &kpAction) {
    bool result = true;
    switch(kpAction.action) {
        case kAction::kActionIncreaseViewWidth :
            OnActionIncreaseWidth();
            break;
        case kAction::kActionDecreaseViewWidth :
            OnActionDecreaseWidth();
            break;
        default:
            result = false;
            break;
    }
    return result;
}

void ViewBase::OnActionIncreaseWidth() {
    auto lhandler = GetLayoutHandler();
    auto w = lhandler->GetWidth();
    lhandler->SetWidth(w + 1);
    RuntimeConfig::Instance().GetRootView().Initialize();
    RuntimeConfig::Instance().GetRootView().InvalidateAll();
}

void ViewBase::OnActionDecreaseWidth() {
    auto lhandler = GetLayoutHandler();
    auto w = lhandler->GetWidth();

    // FIXME: minimum width...
    if (w > 24) {
        lhandler->SetWidth(w - 1);
    }
    RuntimeConfig::Instance().GetRootView().Initialize();
    RuntimeConfig::Instance().GetRootView().InvalidateAll();
}


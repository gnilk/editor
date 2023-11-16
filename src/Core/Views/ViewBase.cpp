//
// Created by gnilk on 21.03.23.
//

#include "ViewBase.h"
#include "Core/Editor.h"
// Ok, need .cpp file for implementation details about MainThread
#include "Core/RuntimeConfig.h"
#include "Core/Runloop.h"

using namespace gedit;

void ViewBase::PostMessage(gedit::ViewBase::MessageCallback callback) {
    if (RuntimeConfig::Instance().IsRootView(this)) {
        Runloop::PostMessage(0x00,[callback](uint32_t id) {
            callback();
        });
    } else {
        RuntimeConfig::Instance().GetRootView().PostMessage(callback);
    }
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
            GetLayoutHandler()->OnActionIncreaseWidth();
            break;
        case kAction::kActionDecreaseViewWidth :
            GetLayoutHandler()->OnActionDecreaseWidth();
            break;
        case kAction::kActionIncreaseViewHeight :
            GetLayoutHandler()->OnActionIncreaseHeight();
            break;
        case kAction::kActionDecreaseViewHeight :
            GetLayoutHandler()->OnActionDecreaseHight();
            break;
        case kAction::kActionMaximizeViewHeight :
            MaximizeContentHeight();
            break;
        default:
            result = false;
            break;
    }
    return result;
}

void ViewBase::OnActionIncreaseWidth() {
    auto w = GetWidth();
    SetWidth(w + 1);
    RuntimeConfig::Instance().GetRootView().Initialize();
    RuntimeConfig::Instance().GetRootView().InvalidateAll();
}

void ViewBase::OnActionDecreaseWidth() {
    auto w = GetWidth();

    // FIXME: minimum width...
    if (w > 24) {
        SetWidth(w - 1);
    }
    RuntimeConfig::Instance().GetRootView().Initialize();
    RuntimeConfig::Instance().GetRootView().InvalidateAll();
}

void ViewBase::OnActionIncreaseHeight() {
    auto w = GetHeight();
    SetHeight(w + 1);
    RuntimeConfig::Instance().GetRootView().Initialize();
    RuntimeConfig::Instance().GetRootView().InvalidateAll();
}

void ViewBase::OnActionDecreaseHight() {

    auto w = GetHeight();
    if (w > 5) {
        SetHeight(w - 1);
    }
    RuntimeConfig::Instance().GetRootView().Initialize();
    RuntimeConfig::Instance().GetRootView().InvalidateAll();
}

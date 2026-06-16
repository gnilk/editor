//
// Created by gnilk on 21.03.23.
//

#include "ViewBase.h"
#include "Core/Editor.h"
#include "Core/UI/UIHost.h"
// Ok, need .cpp file for implementation details about MainThread
#include "Core/Runloop.h"

using namespace gedit;

std::function<void()> ViewBase::layoutChangedHandler = nullptr;

void ViewBase::NotifySessionChanged() {
    if (layoutChangedHandler) {
        layoutChangedHandler();
    }
}

void ViewBase::SetLayoutChangedHandler(std::function<void()> handler) {
    layoutChangedHandler = std::move(handler);
}

void ViewBase::PostMessage(gedit::ViewBase::MessageCallback callback) {
    if (UIHost::Instance().IsRootView(this)) {
        Runloop::PostMessage(0x00,[callback](uint32_t id) {
            callback();
        });
    } else {
        UIHost::Instance().GetRootView().PostMessage(callback);
    }
}

void ViewBase::SetWindowCursor(const Cursor &newCursor) {
    if (Editor::Instance().GetState() == Editor::QuickCommandState) {
        auto quickView = UIHost::Instance().GetQuickCmdView();
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

bool ViewBase::OnAction(const EditorAction &kpAction) {
    bool result = true;
    switch(kpAction.uiAction) {
        case kUIAction::kActionIncreaseViewWidth :
            GetLayoutHandler()->OnActionIncreaseWidth();
            break;
        case kUIAction::kActionDecreaseViewWidth :
            GetLayoutHandler()->OnActionDecreaseWidth();
            break;
        case kUIAction::kActionIncreaseViewHeight :
            GetLayoutHandler()->OnActionIncreaseHeight();
            break;
        case kUIAction::kActionDecreaseViewHeight :
            GetLayoutHandler()->OnActionDecreaseHeight();
            break;
        case kUIAction::kActionMaximizeViewHeight :
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
    UIHost::Instance().GetRootView().Initialize();
    UIHost::Instance().GetRootView().InvalidateAll();
}

void ViewBase::OnActionDecreaseWidth() {
    auto w = GetWidth();

    // FIXME: minimum width...
    if (w > 24) {
        SetWidth(w - 1);
    }
    UIHost::Instance().GetRootView().Initialize();
    UIHost::Instance().GetRootView().InvalidateAll();
}

void ViewBase::OnActionIncreaseHeight() {
    auto h = GetHeight();
    auto logger = gnilk::Logger::GetLogger("Layout");

    logger->Debug("IncreaseHeight for %s@%p %d -> %d", GetClassName().c_str(),(void *)this, h, h+1);

    logger->Info("Before:");
    UIHost::Instance().GetRootView().DumpLayout(0);

    SetHeight(h + 1);
    UIHost::Instance().GetRootView().Initialize();
    UIHost::Instance().GetRootView().InvalidateAll();

    logger->Info("After:");
    UIHost::Instance().GetRootView().DumpLayout(0);
}

void ViewBase::OnActionDecreaseHeight() {

    auto w = GetHeight();
    if (w > 5) {
        SetHeight(w - 1);
    }
    UIHost::Instance().GetRootView().Initialize();
    UIHost::Instance().GetRootView().InvalidateAll();
}

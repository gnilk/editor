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
        auto handler = threadMessages.pop();
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

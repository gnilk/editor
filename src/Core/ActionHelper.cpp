//
// Created by gnilk on 16.05.23.
//

#include "ActionHelper.h"
#include "Core/Editor.h"
#include "Core/UI/Views/RootView.h"

using namespace gedit;

void ActionHelper::SwitchToNextBuffer() {
    auto idxCurrent = Editor::Instance().GetActiveDocumentIndex();
    auto idxNext = Editor::Instance().NextDocumentIndex(idxCurrent);
    if (idxCurrent == idxNext) {
        return;
    }
    Editor::Instance().SetActiveDocumentFromIndex(idxNext);
}

void ActionHelper::SwitchToPreviousBuffer() {
    auto idxCurrent = Editor::Instance().GetActiveDocumentIndex();
    auto idxNext = Editor::Instance().PreviousDocumentIndex(idxCurrent);
    if (idxCurrent == idxNext) {
        return;
    }
    Editor::Instance().SetActiveDocumentFromIndex(idxNext);
}

void ActionHelper::SwitchToNamedView(const std::string &viewName) {
    auto &rvBase = RuntimeConfig::Instance().GetRootView();
    RootView *rootView = static_cast<RootView *>(&rvBase);

    rootView->SetActiveTopViewByName(viewName);
}
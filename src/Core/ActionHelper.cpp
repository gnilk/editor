//
// Created by gnilk on 16.05.23.
//

#include "ActionHelper.h"
#include "Editor.h"
#include "RuntimeConfig.h"

using namespace gedit;

void ActionHelper::SwitchToNextBuffer() {
    auto idxCurrent = Editor::Instance().GetActiveModelIndex();
    auto idxNext = Editor::Instance().NextModelIndex(idxCurrent);
    if (idxCurrent == idxNext) {
        return;
    }
    auto nextModel = Editor::Instance().GetModelFromIndex(idxNext);
    RuntimeConfig::Instance().SetActiveEditorModel(nextModel);
    RuntimeConfig::Instance().GetRootView().Initialize();
}

void ActionHelper::SwitchToPreviousBuffer() {
    auto idxCurrent = Editor::Instance().GetActiveModelIndex();
    auto idxNext = Editor::Instance().PreviousModelIndex(idxCurrent);
    if (idxCurrent == idxNext) {
        return;
    }
    auto nextModel = Editor::Instance().GetModelFromIndex(idxNext);
    RuntimeConfig::Instance().SetActiveEditorModel(nextModel);
    RuntimeConfig::Instance().GetRootView().Initialize();
}

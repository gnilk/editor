//
// Created by gnilk on 16.05.23.
//

#include "ActionHelper.h"
#include "Core/Editor.h"

using namespace gedit;

void ActionHelper::SwitchToNextBuffer() {
    auto idxCurrent = Editor::Instance().GetActiveModelIndex();
    auto idxNext = Editor::Instance().NextModelIndex(idxCurrent);
    if (idxCurrent == idxNext) {
        return;
    }
    Editor::Instance().SetActiveModelFromIndex(idxNext);
}

void ActionHelper::SwitchToPreviousBuffer() {
    auto idxCurrent = Editor::Instance().GetActiveModelIndex();
    auto idxNext = Editor::Instance().PreviousModelIndex(idxCurrent);
    if (idxCurrent == idxNext) {
        return;
    }
    Editor::Instance().SetActiveModelFromIndex(idxNext);
}

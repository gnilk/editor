//
// Created by gnilk on 14.07.23.
//

#include "Editor.h"
#include "UndoHistory.h"

using namespace gedit;

UndoHistory::UndoItem::Ref UndoHistory::NewUndoItem() {
    auto undoItem = std::make_shared<UndoHistory::UndoItem>();
    auto model = Editor::Instance().GetActiveModel();
    if (model == nullptr) {
        return undoItem;
    }
    undoItem->idxLine = model->idxActiveLine;
    undoItem->offset = model->cursor.position.x;
    auto line = model->LineAt(undoItem->idxLine);
    undoItem->data = line->Buffer();    // We are saving the "complete" previous line
    return undoItem;
}


//
// Created by gnilk on 14.07.23.
//

#include "Editor.h"
#include "UndoHistory.h"

using namespace gedit;

UndoHistory::UndoItem::Ref UndoHistory::NewUndoItem() {
    auto undoItem = UndoItemSingle::Create();
    undoItem->Initialize();
    return undoItem;
}

UndoHistory::UndoItem::Ref UndoHistory::NewUndoFromSelection() {
    auto undoItem = UndoItemRange::Create();
    auto model = Editor::Instance().GetActiveModel();
    if (model == nullptr) {
        return undoItem;
    }
    if (!model->IsSelectionActive()) {
        return undoItem;
    }
    auto selection = model->GetSelection();

    undoItem->InitRange(selection.GetStart(), selection.GetEnd());
    return undoItem;
}


void UndoHistory::RestoreOneItem(Cursor &cursor, TextBuffer::Ref textBuffer) {
    auto undoItem = PopItem();
    // Unless we have been initialized properly, we won't restore...
    if (!undoItem->IsValid()) {
        return;
    }
    undoItem->Restore(textBuffer);
    // Update cursor
    cursor.position.x = undoItem->offset;
    cursor.wantedColumn = undoItem->offset;
}

//////////////////
// Baseclass
void UndoHistory::UndoItem::Initialize() {
    auto model = Editor::Instance().GetActiveModel();
    if (model == nullptr) {
        return;
    }
    idxLine = model->idxActiveLine;
    offset = model->cursor.position.x;
    isValid = true;
}

// Single Undo item
UndoHistory::UndoItemSingle::Ref UndoHistory::UndoItemSingle::Create() {
    auto undoItem = std::make_shared<UndoHistory::UndoItemSingle>();
    return undoItem;
}

void UndoHistory::UndoItemSingle::Initialize() {

    UndoHistory::UndoItem::Initialize();

    auto model = Editor::Instance().GetActiveModel();
    if (model == nullptr) {
        return;
    }
    auto line = model->LineAt(idxLine);
    data = line->Buffer();    // We are saving the "complete" previous line
}


void UndoHistory::UndoItemSingle::Restore(TextBuffer::Ref textBuffer) {
    auto line = textBuffer->LineAt(idxLine);
    line->Clear();
    line->Append(data);
}

// Range based undo item
UndoHistory::UndoItemRange::Ref UndoHistory::UndoItemRange::Create() {
    auto undoItem = std::make_shared<UndoHistory::UndoItemRange>();
    return undoItem;
}
void UndoHistory::UndoItemRange::InitRange(const gedit::Point &ptStart, const gedit::Point &ptEnd) {
    UndoHistory::UndoItem::Initialize();

    auto model = Editor::Instance().GetActiveModel();
    if (model == nullptr) {
        return;
    }

    start = ptStart;
    end = ptEnd;

    for(int y=start.y; y < end.y; y++) {
        auto line = model->LineAt(y);
        data.push_back(line);
    }
}
void UndoHistory::UndoItemRange::Restore(TextBuffer::Ref textBuffer) {
    int lineCounter = 0;
    for(int y=start.y; y<end.y;y++) {
        textBuffer->Insert(start.y, data[lineCounter++]);
    }
}



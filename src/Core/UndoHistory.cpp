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

    Point ptStart(0, selection.GetStart().y);
    Point ptEnd(0, selection.GetEnd().y);
    // In case we don't do full-lines, include at least the last line
    // If full-lines, endLine of the selection is one-below..
    if ((selection.GetStart().x != 0) || (selection.GetEnd().x != 0)) {
        ptEnd.y += 1;
    }

    undoItem->InitRange(ptStart, ptEnd);

//    undoItem->InitRange(selection.GetStart(), selection.GetEnd());
    return undoItem;
}

UndoHistory::UndoItem::Ref UndoHistory::NewUndoFromLineRange(size_t idxStartLine, size_t idxEndLine) {
    auto undoItem = UndoItemRange::Create();
    auto model = Editor::Instance().GetActiveModel();
    if (model == nullptr) {
        return undoItem;
    }
    Point ptStart(0, idxStartLine);
    Point ptEnd(0, idxEndLine);
    undoItem->InitRange(ptStart, ptEnd);
    return undoItem;

}


void UndoHistory::RestoreOneItem(Cursor &cursor, size_t &idxActiveLine, TextBuffer::Ref textBuffer) {
    auto undoItem = PopItem();
    // Unless we have been initialized properly, we won't restore...
    if (!undoItem->IsValid()) {
        return;
    }
    undoItem->Restore(textBuffer);
    // Update cursor
    //cursor.position.y = undoItem->;
    idxActiveLine = undoItem->lineCursor.idxActiveLine;
    cursor = undoItem->lineCursor.cursor;
}

//////////////////
// Baseclass
void UndoHistory::UndoItem::Initialize() {
    auto model = Editor::Instance().GetActiveModel();
    if (model == nullptr) {
        return;
    }
    lineCursor = model->GetLineCursor();
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
    auto line = model->LineAt(lineCursor.idxActiveLine);
    data = line->Buffer();    // We are saving the "complete" previous line
}


void UndoHistory::UndoItemSingle::Restore(TextBuffer::Ref textBuffer) {
    auto line = textBuffer->LineAt(lineCursor.idxActiveLine);
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
        data.push_back(line->Buffer());
    }
}
void UndoHistory::UndoItemRange::Restore(TextBuffer::Ref textBuffer) {

    //
    // Clear and append, restore previous line contents without
    //
    if (action == kRestoreAction::kClearAndAppend) {
        auto idxLine = start.y;
        for(auto &oldLine : data) {
            auto line = textBuffer->LineAt(idxLine);
            line->Clear();
            line->Append(oldLine);
            idxLine++;
        }
        return;
    }

    auto logger = gnilk::Logger::GetLogger("UndoItemRange");
    logger->Debug("Restore, start.y = %zu, end.y = %zu, action=%d", start.y, end.y, action);

    // Action used when pasting from clip-board
    // Basically this is enough - just remove whatever we had
    if (action == kRestoreAction::kDeleteBeforeInsert) {
        for (int y = start.y; y < end.y; y++) {
            textBuffer->DeleteLineAt(start.y);
        }
    } else if (action == kRestoreAction::kDeleteFirstBeforeInsert) {
        textBuffer->DeleteLineAt(start.y);
    }

    // But we also restore the contents - thus I need to remove a line for every-one I insert..
    // This will literally delete and replace them same content (unless we have 'overwrite' mode - which I don't support anyway)
    while(!data.empty()) {
        if (action == kRestoreAction::kDeleteBeforeInsert) {
            textBuffer->DeleteLineAt(start.y);
        }
        auto line = Line::Create(data.back());
        textBuffer->Insert(start.y, line);
        data.pop_back();
    }
/*

    int lineCounter = 0;
    for(int y=start.y; y<end.y+1;y++) {
        if (action == kRestoreAction::kInsertAsNew) {
            auto line = Line::Create(data[lineCounter++]);
            //textBuffer->Insert(start.y, line);
            textBuffer->Insert(start.y, line);
        } else {
            auto line = textBuffer->LineAt(y);
            line->Clear();
            line->Append(data[lineCounter++]);
        }
    }
*/
}



//
// Created by gnilk on 14.07.23.
//

#include "UndoHistory.h"

using namespace gedit;

UndoHistory::UndoItem::Ref UndoHistory::NewUndoItem(const LineCursor &fromCursor, TextBuffer::Ref fromBuffer) {
    auto undoItem = UndoItemSingle::Create();
    undoItem->Initialize(fromCursor, fromBuffer);
    return undoItem;
}

UndoHistory::UndoItem::Ref UndoHistory::NewUndoFromSelection(const LineCursor &fromCursor, const Selection &fromSelection, TextBuffer::Ref fromBuffer) {
    auto undoItem = UndoItemRange::Create();
    if (!fromSelection.IsActive()) {
        return undoItem;
    }

    Point ptStart(0, fromSelection.GetStart().y);
    Point ptEnd(0, fromSelection.GetEnd().y);
    // In case we don't do full-lines, include at least the last line
    // If full-lines, endLine of the selection is one-below..
    if ((fromSelection.GetStart().x != 0) || (fromSelection.GetEnd().x != 0)) {
        ptEnd.y += 1;
    }

    undoItem->InitRange(ptStart, ptEnd, fromCursor, fromBuffer);
    return undoItem;
}

UndoHistory::UndoItem::Ref UndoHistory::NewUndoFromLineRange(size_t idxStartLine, size_t idxEndLine, const LineCursor &fromCursor, TextBuffer::Ref fromBuffer) {
    auto undoItem = UndoItemRange::Create();
    Point ptStart(0, idxStartLine);
    Point ptEnd(0, idxEndLine);
    undoItem->InitRange(ptStart, ptEnd, fromCursor, fromBuffer);
    return undoItem;
}


int32_t UndoHistory::RestoreOneItem(Cursor &cursor, size_t &idxActiveLine, TextBuffer::Ref textBuffer) {
    auto undoItem = PopItem();
    // Unless we have been initialized properly, we won't restore...
    if (!undoItem->IsValid()) {
        return 0;
    }
    int32_t nItemsRestored = undoItem->Restore(textBuffer);
    // Update cursor
    //cursor.position.y = undoItem->;
    idxActiveLine = undoItem->lineCursor.idxActiveLine;
    cursor = undoItem->lineCursor.cursor;

    return nItemsRestored;
}

//////////////////
// Baseclass
void UndoHistory::UndoItem::Initialize(const LineCursor &fromCursor) {
    lineCursor = fromCursor;
    isValid = true;
}

// Single Undo item
UndoHistory::UndoItemSingle::Ref UndoHistory::UndoItemSingle::Create() {
    auto undoItem = std::make_shared<UndoHistory::UndoItemSingle>();
    return undoItem;
}

void UndoHistory::UndoItemSingle::Initialize(const LineCursor &fromCursor, TextBuffer::Ref fromBuffer) {

    UndoHistory::UndoItem::Initialize(fromCursor);

    if (fromBuffer == nullptr) {
        return;
    }
    auto line = fromBuffer->LineAt(lineCursor.idxActiveLine);
    data = line->Buffer();    // We are saving the "complete" previous line
}


int32_t UndoHistory::UndoItemSingle::Restore(TextBuffer::Ref textBuffer) {
    auto line = textBuffer->LineAt(lineCursor.idxActiveLine);
    line->Clear();
    line->Append(data);
    return 1;
}

// Range based undo item
UndoHistory::UndoItemRange::Ref UndoHistory::UndoItemRange::Create() {
    auto undoItem = std::make_shared<UndoHistory::UndoItemRange>();
    return undoItem;
}
void UndoHistory::UndoItemRange::InitRange(const gedit::Point &ptStart, const gedit::Point &ptEnd, const LineCursor &fromCursor, TextBuffer::Ref fromBuffer) {
    UndoHistory::UndoItem::Initialize(fromCursor);

    if (fromBuffer == nullptr) {
        return;
    }

    // Clip against the supplied buffer..
    start = ptStart;
    end = ptEnd;

    for(int y=start.y; y < end.y; y++) {
        auto line = fromBuffer->LineAt(y);
        // end of lines?
        if (line == nullptr) {
            break;
        }
        data.push_back(line->Buffer());
    }
}
int32_t UndoHistory::UndoItemRange::Restore(TextBuffer::Ref textBuffer) {

    int nLinesToRestore = data.size();

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
        return nLinesToRestore;
    }

    // General range replace: the edit left 'replaceCount' lines where 'data' originally sat (the counts may
    // differ - e.g. a block-surround turned N body lines into N+2). Delete those, restore the saved originals.
    if (action == kRestoreAction::kReplaceRange) {
        for (int32_t i = 0; i < replaceCount; i++) {
            textBuffer->DeleteLineAt(start.y);
        }
        for (size_t i = 0; i < data.size(); i++) {
            textBuffer->Insert(start.y + i, Line::Create(data[i]));
        }
        return nLinesToRestore;
    }

    auto logger = gnilk::Logger::GetLogger("UndoItemRange");
    logger->Debug("Restore, start.y = %zu, end.y = %zu, action=%d", start.y, end.y, (int)action);

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

    return nLinesToRestore;
}



//
// Created by gnilk on 15.02.23.
//
// Not sure this class makes much sense anymore - moved almost anything doing 'document->XYZ' already to document
//
//

#include "EditController.h"
#include "Core/UndoHistory.h"
#include "Core/Editor.h"
#include "Core/Rect.h"
#include "Core/Language/AutoPairCache.h"
#include <sstream>
#include <memory>

using namespace gedit;

// Token class covering a cursor column - drives auto-pair suppression in strings/comments and quote
// open-vs-skip. Falls back to kRegular for an unparsed/empty line or a cursor past the last span.
static kLanguageTokenClass TokenClassAt(const Line::Ref &line, int x) {
    auto &attribs = line->Attributes();
    if (attribs.empty()) {
        return kLanguageTokenClass::kRegular;
    }
    auto it = line->AttributeAt(x);
    if (it == attribs.end()) {
        return kLanguageTokenClass::kRegular;
    }
    return it->tokenClass;
}

EditController::Ref EditController::Create(const Document::Ref &newDocument) {
    auto inst = std::make_shared<EditController>();
    inst->Attach(newDocument);
    return inst;
}


void EditController::Begin() {
    if (logger == nullptr) {
        logger = gnilk::Logger::GetLogger("EditController");
    }
}


bool EditController::HandleKeyPress(Cursor &cursor, size_t &idxLine, const KeyPress &keyPress) {
    if (!document) {
        return false;
    }
    if (!document->GetTextBuffer()) {
        return false;
    }

    auto textBuffer = document->GetTextBuffer();

    // Keep the inherited single-line edit helpers (AddCharToLine etc.) capturing a *visual*
    // wanted-column using the active language's tab width.
    SetEditTabSize(document->GetTabSize());

    if (textBuffer->IsReadOnly()) {
        return false;
    }

    auto line = textBuffer->LineAt(idxLine);
    if (line == nullptr) {
        logger->Error("Line is null, idxLine=%zu, cursor=(%d,%d)", idxLine, cursor.position.x, cursor.position.y);
        return false;
    }


    auto undoItem = document->BeginUndoItem();

    // Auto-pairing: for a printable char the engine decides whether to insert a pair, step over an
    // existing closer, or do nothing. (Selection-wrap is decided upstream in OnKeyPress; not wired yet.)
    if (keyPress.IsHumanReadable()) {
        auto action = AutoPairEngine::OnInsertChar(BuildAutoPairContext(line, cursor, false), keyPress.key);
        if (action.type == AutoPairEngine::kPairAction::kSkipOver) {
            // Type-through: insert nothing, just step the cursor over the closer already to the right.
            cursor.position.x++;
            cursor.wantedColumn = line->CharToVisualColumn(cursor.position.x, document->GetTabSize());
            document->EndUndoItem(undoItem);
            return true;
        }
        if (action.type == AutoPairEngine::kPairAction::kInsertPair) {
            // Insert the opener (DefaultEditLine advances cursor + wantedColumn), then drop the closer
            // after the cursor so it lands between the pair.
            if (DefaultEditLine(cursor, line, keyPress, false)) {
                line->Insert(cursor.position.x, action.close);
                document->EndUndoItem(undoItem);
                document->UpdateSyntaxForActiveLineRegion();
                return true;
            }
        }
    }

    // Default: just insert the typed char.
    if (DefaultEditLine(cursor, line, keyPress, false)) {
        document->EndUndoItem(undoItem);
        document->UpdateSyntaxForActiveLineRegion();
        return true;
    }

    return false;
}

bool EditController::HandleSpecialKeyPress(Cursor &cursor, size_t &idxLine, const KeyPress &keyPress) {
    auto textBuffer = document->GetTextBuffer();
    auto line = textBuffer->LineAt(idxLine);
    auto undoItem = document->BeginUndoItem();
    bool wasHandled = true;

    // Auto-pairing: backspace with the cursor between an empty pair "()" deletes both halves.
    if ((keyPress.specialKey == Keyboard::kKeyCode_Backspace) && (line != nullptr)) {
        auto action = AutoPairEngine::OnBackspace(BuildAutoPairContext(line, cursor, false));
        if (action.type == AutoPairEngine::kPairAction::kDeletePair) {
            line->Delete(cursor.position.x);        // the closer at the cursor
            cursor.position.x--;
            line->Delete(cursor.position.x);        // the opener before the cursor
            cursor.wantedColumn = line->CharToVisualColumn(cursor.position.x, document->GetTabSize());
            document->EndUndoItem(undoItem);
            document->UpdateSyntaxForActiveLineRegion();
            return true;
        }
    }

    if (DefaultEditSpecial(cursor, line, keyPress)) {
        document->EndUndoItem(undoItem);
    } else {
        // Just drop the undo-item, handle special key must declare it's own...
        wasHandled = HandleSpecialKeyPressForEditor(cursor, idxLine, keyPress);
    }
    document->UpdateSyntaxForActiveLineRegion();
    return wasHandled;
}

bool EditController::HandleSpecialKeyPressForEditor(Cursor &cursor, size_t &idxLine, const KeyPress &keyPress) {
    auto textBuffer = document->GetTextBuffer();
    auto line = textBuffer->LineAt(idxLine);
    bool wasHandled = false;
    switch (keyPress.specialKey) {
        case Keyboard::kKeyCode_DeleteForward :
            // Handle delete at end of line
            if ((cursor.position.x == (int)line->Length()) && ((idxLine + 1) < textBuffer->NumLines())) {
                auto undoItem = document->BeginUndoFromLineRange(idxLine, idxLine+2);
                undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteFirstBeforeInsert);

                auto next = textBuffer->LineAt(idxLine + 1);
                line->Append(next);
                textBuffer->DeleteLineAt(idxLine + 1);

                document->EndUndoItem(undoItem);
                wasHandled = true;
            }
            break;
        case Keyboard::kKeyCode_Backspace :
            if ((cursor.position.x == 0) && (idxLine > 0)) {
                auto undoItem = document->BeginUndoFromLineRange(idxLine-1, idxLine+1);
                undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteFirstBeforeInsert);
                MoveLineUp(cursor, idxLine);
                document->EndUndoItem(undoItem);
                wasHandled = true;
            }
            break;
    }
    return wasHandled;
}

void EditController::MoveLineUp(Cursor &cursor, size_t &idxActiveLine) {
    auto textBuffer = document->GetTextBuffer();
    auto line = textBuffer->LineAt(idxActiveLine);
    auto linePrevious = textBuffer->LineAt((idxActiveLine-1));

    cursor.wantedColumn = linePrevious->CharToVisualColumn(linePrevious->Length(), document->GetTabSize());
    linePrevious->Append(line);
    textBuffer->DeleteLineAt(idxActiveLine);
    idxActiveLine--;
    cursor.position.y--;
}

// Newly moved stuff from EditorView
bool EditController::OnKeyPress(const KeyPress &keyPress) {
    // This can all be pushed to controller / document
    if (document == nullptr) {
        return false;
    }
    // Unless we can edit - we do nothing
    if (!document->GetTextBuffer()->CanEdit()) return false;

    // Auto-pairing: typing an opener/quote OVER a selection surrounds it instead of replacing it. Must run
    // before the delete-and-insert below, since that would discard the selection content.
    if (document->IsSelectionActive() && keyPress.IsHumanReadable()) {
        if (TryWrapSelection(keyPress.key)) {
            document->UpdateDocumentFromNavigation(true);
            return true;
        }
    }

    // In case we have selection active - we treat the whole thing a bit differently...
    if (document->IsSelectionActive()) {

        document->DeleteSelection();
        document->RestoreCursorFromSelection();
        document->CancelSelection();

        if ((keyPress.specialKey == Keyboard::kKeyCode_Backspace) || (keyPress.specialKey == Keyboard::kKeyCode_DeleteForward)) {
            return true;
        }
    }

    auto &lineCursor = document->GetLineCursor();

    // Let the controller have a go - this is regular editing and so forth
    if (HandleKeyPress(lineCursor.cursor, lineCursor.idxActiveLine, keyPress)) {
        document->UpdateDocumentFromNavigation(true);
        return true;
    }

    // This handles regular backspace/delete/home/end (which are default actions for any single-line editing)
    if (HandleSpecialKeyPress(lineCursor.cursor, lineCursor.idxActiveLine, keyPress)) {
        document->UpdateDocumentFromNavigation(true);
        return true;
    }

    return false;
}

// The single place that turns editor state into an AutoPairEngine::Context: the table is selected by the
// language's config name (the autopairs.yml key; empty/unknown => empty table => no pairing), the line text
// + cursor come from the active edit, and the token class at the cursor gates string/comment suppression.
AutoPairEngine::Context EditController::BuildAutoPairContext(const Line::Ref &line, const Cursor &cursor, bool selectionActive) {
    AutoPairEngine::Context ctx;
    auto &language = document->GetTextBuffer()->GetLanguage();
    ctx.table = AutoPairCache::Instance().GetTableForLanguage(language.GetConfigNodeName()).get();
    ctx.lineText = line->Buffer();
    ctx.cursorX = cursor.position.x;
    ctx.selectionActive = selectionActive;
    ctx.tokenClassAtCursor = TokenClassAt(line, cursor.position.x);
    return ctx;
}

bool EditController::TryWrapSelection(char32_t typed) {
    auto &selection = document->GetSelection();
    if (!selection.IsActive()) {
        return false;
    }
    auto start = selection.GetStart();      // sorted top-left (buffer coords: x=char, y=line)
    auto end = selection.GetEnd();          // sorted bottom-right

    auto textBuffer = document->GetTextBuffer();
    auto startLine = textBuffer->LineAt(start.y);
    auto endLine = textBuffer->LineAt(end.y);
    if ((startLine == nullptr) || (endLine == nullptr)) {
        return false;
    }

    // Ask the engine (selectionActive=true) - only an opener/quote yields a wrap; anything else falls back.
    Cursor at;
    at.position = start;
    auto action = AutoPairEngine::OnInsertChar(BuildAutoPairContext(startLine, at, true), typed);
    if (action.type != AutoPairEngine::kPairAction::kWrapSelection) {
        return false;
    }

    auto undoItem = document->BeginUndoFromLineRange(start.y, end.y + 1);

    // Insert the closer first (at the selection end), then the opener at the start - in this order the
    // opener insert doesn't shift the closer's position mid-operation.
    endLine->Insert(end.x, action.close);
    startLine->Insert(start.x, action.open);

    document->EndUndoItem(undoItem);

    // Final closer index: on a single-line selection the opener shifted it right by one; across lines it
    // is untouched. Park the cursor just after it and drop the selection.
    int closeIndex = (end.y == start.y) ? end.x + 1 : end.x;
    document->CancelSelection();
    auto &lineCursor = document->GetLineCursor();
    lineCursor.idxActiveLine = end.y;
    lineCursor.cursor.position = { closeIndex + 1, end.y };
    lineCursor.cursor.wantedColumn = endLine->CharToVisualColumn(closeIndex + 1, document->GetTabSize());

    document->UpdateSyntaxForRegion(start.y, end.y + 1);
    return true;
}

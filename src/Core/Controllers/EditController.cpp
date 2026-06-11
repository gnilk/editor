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

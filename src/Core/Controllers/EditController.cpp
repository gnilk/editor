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
#include <sstream>
#include <memory>

using namespace gedit;

EditController::Ref EditController::Create(Document::Ref newDocument) {
    auto inst = std::make_shared<EditController>(newDocument);
    inst->Begin();
    return inst;
}


void EditController::Begin() {
    if (logger == nullptr) {
        logger = gnilk::Logger::GetLogger("EditController");
    }
}

void EditController::OnViewInit(const Rect &viewRect) {
    document->OnViewInit(viewRect);
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
    LanguageBase::kInsertAction parserAction = LanguageBase::kInsertAction::kDefault;

    // FIXME: rename!!!!
    bool doPrePostInsert = Config::Instance()["editorview"].GetBool("enable_pre_post_insert", true);


    if (keyPress.IsHumanReadable() && doPrePostInsert) {
        parserAction = textBuffer->GetLanguage().OnPreInsertChar(cursor, line, keyPress.key);
    }
    // The pre-insert handler for a language can determine if we should 'stop' the default behavior..
    if (parserAction == LanguageBase::kInsertAction::kNoInsert) {
        document->EndUndoItem(undoItem);
        return true;
    }

    // Except for this line - all things belong to the document - more or less...
    if ((parserAction == LanguageBase::kInsertAction::kDefault) && DefaultEditLine(cursor, line, keyPress, false)) {
        if (keyPress.IsHumanReadable() && doPrePostInsert) {
            textBuffer->GetLanguage().OnPostInsertChar(cursor, line, keyPress.key);
        }
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

bool EditController::OnAction(const KeyPressAction &kpAction) {
    // Move to controller
    if (document == nullptr) {
        return false;
    }
    // Dispatch this directly to the document
    return document->OnAction(kpAction);
}

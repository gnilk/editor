//
// Created by gnilk on 15.02.23.
//
// Not sure this class makes much sense anymore - moved almost anything doing 'model->XYZ' already to model
//
//

#include "EditController.h"
#include "Core/UndoHistory.h"
#include "Core/Editor.h"
#include "Core/Rect.h"
#include <sstream>
#include <memory>

using namespace gedit;

EditController::Ref EditController::Create(EditorModel::Ref newModel) {
    auto inst = std::make_shared<EditController>(newModel);
    inst->Begin();
    return inst;
}


void EditController::Begin() {
    if (logger == nullptr) {
        logger = gnilk::Logger::GetLogger("EditController");
    }
}

void EditController::OnViewInit(const Rect &viewRect) {
    model->OnViewInit(viewRect);
}


bool EditController::HandleKeyPress(Cursor &cursor, size_t &idxLine, const KeyPress &keyPress) {
    if (!model) {
        return false;
    }
    if (!model->GetTextBuffer()) {
        return false;
    }

    auto textBuffer = model->GetTextBuffer();

    if (textBuffer->IsReadOnly()) {
        return false;
    }

    auto line = textBuffer->LineAt(idxLine);
    if (line == nullptr) {
        logger->Error("Line is null, idxLine=%zu, cursor=(%d,%d)", idxLine, cursor.position.x, cursor.position.y);
        return false;
    }
    auto undoItem = model->BeginUndoItem();

    LanguageBase::kInsertAction parserAction = LanguageBase::kInsertAction::kDefault;

    if (keyPress.IsHumanReadable()) {
        parserAction = textBuffer->GetLanguage().OnPreInsertChar(cursor, line, keyPress.key);
    }
    // The pre-insert handler for a language can determine if we should 'stop' the default behavior..
    if (parserAction == LanguageBase::kInsertAction::kNoInsert) {
        model->EndUndoItem(undoItem);
        return true;
    }

    if ((parserAction == LanguageBase::kInsertAction::kDefault) && DefaultEditLine(cursor, line, keyPress, false)) {
        if (keyPress.IsHumanReadable()) {
            textBuffer->GetLanguage().OnPostInsertChar(cursor, line, keyPress.key);
        }
        model->EndUndoItem(undoItem);
        model->UpdateSyntaxForActiveLineRegion();
        return true;
    }

    return false;
}

bool EditController::HandleSpecialKeyPress(Cursor &cursor, size_t &idxLine, const KeyPress &keyPress) {
    auto textBuffer = model->GetTextBuffer();
    auto line = textBuffer->LineAt(idxLine);
    auto undoItem = model->BeginUndoItem();
    bool wasHandled = true;

    if (DefaultEditSpecial(cursor, line, keyPress)) {
        model->EndUndoItem(undoItem);
    } else {
        // Just drop the undo-item, handle special key must declare it's own...
        wasHandled = HandleSpecialKeyPressForEditor(cursor, idxLine, keyPress);
    }
    model->UpdateSyntaxForActiveLineRegion();
    return wasHandled;
}

bool EditController::HandleSpecialKeyPressForEditor(Cursor &cursor, size_t &idxLine, const KeyPress &keyPress) {
    auto textBuffer = model->GetTextBuffer();
    auto line = textBuffer->LineAt(idxLine);
    bool wasHandled = false;
    switch (keyPress.specialKey) {
        case Keyboard::kKeyCode_DeleteForward :
            // Handle delete at end of line
            if ((cursor.position.x == (int)line->Length()) && ((idxLine + 1) < textBuffer->NumLines())) {
                auto undoItem = model->BeginUndoFromLineRange(idxLine, idxLine+2);
                undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteFirstBeforeInsert);

                auto next = textBuffer->LineAt(idxLine + 1);
                line->Append(next);
                textBuffer->DeleteLineAt(idxLine + 1);

                model->EndUndoItem(undoItem);
                wasHandled = true;
            }
            break;
        case Keyboard::kKeyCode_Backspace :
            if ((cursor.position.x == 0) && (idxLine > 0)) {
                auto undoItem = model->BeginUndoFromLineRange(idxLine-1, idxLine+1);
                undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteFirstBeforeInsert);
                MoveLineUp(cursor, idxLine);
                model->EndUndoItem(undoItem);
                wasHandled = true;
            }
            break;
        case Keyboard::kKeyCode_Tab :
            {
                auto undoItem = model->BeginUndoItem();
                if (keyPress.modifiers == 0) {
                    model->AddTab(cursor, idxLine);
                } else if (keyPress.IsShiftPressed()) {
                    model->DelTab(cursor, idxLine);
                }
                model->EndUndoItem(undoItem);
            }

            wasHandled = true;
            break;
    }
    return wasHandled;
}

void EditController::MoveLineUp(Cursor &cursor, size_t &idxActiveLine) {
    auto textBuffer = model->GetTextBuffer();
    auto line = textBuffer->LineAt(idxActiveLine);
    auto linePrevious = textBuffer->LineAt((idxActiveLine-1));

    cursor.wantedColumn = linePrevious->Length();
    linePrevious->Append(line);
    textBuffer->DeleteLineAt(idxActiveLine);
    idxActiveLine--;
    cursor.position.y--;
}

// Newly moved stuff from EditorView
bool EditController::OnKeyPress(const KeyPress &keyPress) {
    // This can all be pushed to controller / model
    if (model == nullptr) {
        return false;
    }
    // Unless we can edit - we do nothing
    if (!model->GetTextBuffer()->CanEdit()) return false;

    // In case we have selection active - we treat the whole thing a bit differently...
    if (model->IsSelectionActive()) {

        auto &lineCursor = model->GetLineCursor();
        auto &selection = model->GetSelection();
        model->DeleteSelection();
        model->CancelSelection();

        lineCursor.idxActiveLine = selection.GetStart().y;
        lineCursor.cursor.position = selection.GetStart();
        if ((keyPress.specialKey == Keyboard::kKeyCode_Backspace) || (keyPress.specialKey == Keyboard::kKeyCode_DeleteForward)) {
            return true;
        }
    }

    auto &lineCursor = model->GetLineCursor();

    // Let the controller have a go - this is regular editing and so forth
    if (HandleKeyPress(lineCursor.cursor, lineCursor.idxActiveLine, keyPress)) {
        model->UpdateModelFromNavigation(true);
        return true;
    }

    // This handles regular backspace/delete/home/end (which are default actions for any single-line editing)
    if (HandleSpecialKeyPress(lineCursor.cursor, lineCursor.idxActiveLine, keyPress)) {
        model->UpdateModelFromNavigation(true);
        return true;
    }

    return false;
}

bool EditController::OnAction(const KeyPressAction &kpAction) {
    // Move to controller
    if (model == nullptr) {
        return false;
    }
    // Dispatch this directly to the model
    return model->OnAction(kpAction);
}

//
// Created by gnilk on 26.04.23.
//

#include "EditorModel.h"
#include "EditorConfig.h"

using namespace gedit;

bool EditorModel::HandleKeyPress(const gedit::KeyPress &keyPress) {
    bool wasHandled = true;
    auto line = textBuffer->LineAt(idxActiveLine);
    switch (keyPress.specialKey) {
        case Keyboard::kKeyCode_DeleteForward :
            if (IsSelectionActive()) {
                DeleteSelection();
                CancelSelection();

            } else if ((cursor.position.x == line->Length()) && ((idxActiveLine + 1) < textBuffer->NumLines())) {
                    auto next = textBuffer->LineAt(idxActiveLine + 1);
                    line->Append(next);
                    textBuffer->DeleteLineAt(idxActiveLine + 1);
            } else {
                wasHandled = false;
            }

            break;
        case Keyboard::kKeyCode_Backspace :
            if ((cursor.position.x == 0) && (idxActiveLine > 0)) {
                MoveLineUp();
            } else {
                wasHandled = false;
            }
            break;
        case Keyboard::kKeyCode_Tab :
            // FIXME: Handle selection..
            if (keyPress.modifiers == 0) {
                for (int i = 0; i < EditorConfig::Instance().tabSize; i++) {
                    editController->AddCharToLine(cursor, line, ' ');
                }
            } else if (keyPress.IsShiftPressed()) {
                for (int i = 0; i < EditorConfig::Instance().tabSize; i++) {
                    editController->RemoveCharFromLine(cursor, line);
                }
            }

            break;
        default:
            wasHandled = false;
            break;
    }

    if (wasHandled) {
        textBuffer->Reparse();
    }

    return wasHandled;
}

void EditorModel::MoveLineUp() {
    auto line = textBuffer->LineAt(idxActiveLine);
    auto linePrevious = textBuffer->LineAt((idxActiveLine-1));
    cursor.position.x = linePrevious->Length();
    linePrevious->Append(line);
    textBuffer->DeleteLineAt(idxActiveLine);
    idxActiveLine--;
    cursor.position.y--;
}

void EditorModel::DeleteSelection() {
    auto startPos = currentSelection.GetStart();
    auto endPos = currentSelection.GetEnd();

    int yStart = startPos.y;
    int yEnd = endPos.y;
    if (startPos.x > 0) {
        yStart++;
    }
    if (endPos.x > 0) {
        yEnd--;
    }
    for(int lineIndex = yStart;lineIndex < yEnd; lineIndex++) {
        // delete line = lineIndex;
        textBuffer->DeleteLineAt(yStart);
    }
    idxActiveLine = yStart;
    cursor.position.y = yStart;

}

void EditorModel::CommentSelectionOrLine() {

    if (!textBuffer->HaveLanguage()) {
        return;
    }
    auto lineCommentPrefix = textBuffer->LangParser().GetLineComment();
    if (lineCommentPrefix.empty()) {
        return;
    }

    if (!IsSelectionActive()) {
        auto line = LineAt(idxActiveLine);
        if (!line->StartsWith(lineCommentPrefix)) {
            line->Insert(0, lineCommentPrefix);
        } else {
            line->Delete(0,2);
        }
        textBuffer->Reparse();
        return;
    }
    auto start = currentSelection.GetStart();
    auto end = currentSelection.GetEnd();
    int y = start.y;
    while (y < end.y) {
        auto line = LineAt(y);
        if (!line->StartsWith(lineCommentPrefix)) {
            line->Insert(0, lineCommentPrefix);
        } else {
            line->Delete(0,2);
        }
        y++;
    }
    textBuffer->Reparse();

}




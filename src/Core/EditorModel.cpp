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
            if ((cursor.position.x == line->Length()) && ((idxActiveLine+1) < textBuffer->NumLines())) {

                auto next = textBuffer->LineAt(idxActiveLine+1);
                line->Append(next);
                textBuffer->DeleteLineAt(idxActiveLine+1);
                wasHandled = true;
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


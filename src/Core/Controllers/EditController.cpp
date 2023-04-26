//
// Created by gnilk on 15.02.23.
//

#include "EditController.h"
#include "Core/EditorConfig.h"

using namespace gedit;

void EditController::Begin() {
    if (logger == nullptr) {
        logger = gnilk::Logger::GetLogger("EditorController");
    }
}

void EditController::SetTextBuffer(TextBuffer::Ref newTextBuffer) {
    textBuffer = newTextBuffer;
    if (onTextBufferChanged != nullptr) {
        onTextBufferChanged();
    }
}


bool EditController::HandleKeyPress(Cursor &cursor, size_t idxLine, const KeyPress &keyPress) {
    if (!textBuffer) {
        return false;
    }
    // Can't just be this blunt - because SHIFT+<number> won't work...
    //if (keyPress.modifiers) return false;

    auto line = textBuffer->LineAt(idxLine);
    if (keyPress.IsHumanReadable()) {
        textBuffer->LangParser().OnPreInsertChar(cursor, line, keyPress.key);
    }
    if (DefaultEditLine(cursor, line, keyPress)) {
        if (keyPress.IsHumanReadable()) {
            textBuffer->LangParser().OnPostInsertChar(cursor, line, keyPress.key);
        }
        return true;
    }

    // FIXME: Handle backspace in case of cursor.position.x == 0 (i.e. move the line up to the next)
    if (!keyPress.isSpecialKey) {
        return false;
    }
    bool wasHandled = true;
    switch (keyPress.specialKey) {
        case Keyboard::kKeyCode_Tab :
            // FIXME: Handle selection..
            if (keyPress.modifiers == 0) {
                for (int i = 0; i < EditorConfig::Instance().tabSize; i++) {
                    AddCharToLine(cursor, line, ' ');
                }
            } else if (keyPress.IsShiftPressed()) {
                for (int i = 0; i < EditorConfig::Instance().tabSize; i++) {
                    RemoveCharFromLine(cursor, line);
                }
            }

            break;

        default:
            wasHandled = false;
            break;
    }

    return wasHandled;
}


size_t EditController::NewLine(size_t idxActiveLine, Cursor &cursor) {
    int indentPrevious = 0;
    auto &lines = Lines();
    auto currentLine = LineAt(idxActiveLine);

    if (currentLine != nullptr) {
        auto tabSize = EditorConfig::Instance().tabSize;
        indentPrevious = currentLine->Indent() * tabSize;
    }

    auto it = lines.begin() + idxActiveLine;
    if (lines.size() == 0) {
        it = lines.insert(it, new Line());
    } else {
        if (cursor.position.x == 0) {
            // Insert empty line...
            idxActiveLine++;
            indentPrevious = 0;
        } else {
            // Split, move some chars from current to new...
            auto newLine = new Line();
            currentLine->Move(newLine, 0, cursor.position.x);
            idxActiveLine++;
        }
    }

    currentLine = lines[idxActiveLine];
    currentLine->SetIndent(indentPrevious);
    int cursorPos = currentLine->Insert(0, indentPrevious, ' ');

    // FIXME: Do not invalidate the whole buffer
    UpdateSyntaxForBuffer();

    cursor.wantedColumn = cursorPos;
    cursor.position.x = cursorPos;
    return idxActiveLine;
}

void EditController::UpdateSyntaxForBuffer() {
    textBuffer->Reparse();
}

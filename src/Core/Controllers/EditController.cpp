//
// Created by gnilk on 15.02.23.
//
// The controller is in charge of MODIFYING the underlying textbuffer - any such function present in the Model should
// be moved here - see comment in EditorModel.cpp
//

#include "EditController.h"
#include "Core/EditorConfig.h"
#include <sstream>

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

    auto undoItem = historyBuffer.NewUndoItem();
    undoItem->idxLine = idxLine;
    undoItem->offset = cursor.position.x;
    undoItem->data = line->Buffer();    // We are saving the "complete" previous line


    if (keyPress.IsHumanReadable()) {
        textBuffer->LangParser().OnPreInsertChar(cursor, line, keyPress.key);
    }
    if (DefaultEditLine(cursor, line, keyPress, false)) {
        if (keyPress.IsHumanReadable()) {
            textBuffer->LangParser().OnPostInsertChar(cursor, line, keyPress.key);
        }
        historyBuffer.PushUndoItem(undoItem);
        return true;
    }

    return false;
}

bool EditController::HandleSpecialKeyPress(Cursor &cursor, size_t idxLine, const KeyPress &keyPress) {
    auto line = textBuffer->LineAt(idxLine);

    auto undoItem = historyBuffer.NewUndoItem();
    undoItem->idxLine = idxLine;
    undoItem->offset = cursor.position.x;
    undoItem->data = line->Buffer();    // We are saving the "complete" previous line

    if (!DefaultEditSpecial(cursor, line, keyPress)) {
        return false;
    }
    historyBuffer.PushUndoItem(undoItem);
    UpdateSyntaxForBuffer();


    return true;
}

void EditController::Undo(Cursor &cursor) {
    if (!historyBuffer.HaveHistory()) {
        return;
    }
    historyBuffer.Dump();
    auto undoItem = historyBuffer.PopItem();
    auto line = textBuffer->LineAt(undoItem->idxLine);
    line->Clear();
    line->Append(undoItem->data);

    // Update cursor
    cursor.position.x = undoItem->offset;
    cursor.wantedColumn = undoItem->offset;

    UpdateSyntaxForBuffer();
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
        lines.insert(it, std::make_shared<Line>());
    } else {
        if (cursor.position.x == 0) {
            // Insert empty line...
            lines.insert(it, std::make_shared<Line>());
            idxActiveLine++;
            indentPrevious = 0;
        } else {
            // Split, move some chars from current to new...
            auto newLine = std::make_shared<Line>();
            currentLine->Move(newLine, 0, cursor.position.x);
            lines.insert(it + 1, newLine);
            idxActiveLine++;
        }
    }

    currentLine = lines[idxActiveLine];
    currentLine->SetIndent(indentPrevious);
    int cursorPos = currentLine->Insert(0, indentPrevious, ' ');

    UpdateSyntaxForBuffer();

    cursor.wantedColumn = cursorPos;
    cursor.position.x = cursorPos;
    return idxActiveLine;
}

// FIX-ME: This needs more info - like the cursor in order to paste the block middle of another block..
void EditController::Paste(size_t idxActiveLine, const char *buffer) {
    std::stringstream strStream(buffer);
    char tmp[GEDIT_MAX_LINE_LENGTH];

    while(!strStream.eof()) {
        strStream.getline(tmp, GEDIT_MAX_LINE_LENGTH);
        auto line = Line::Create(tmp);
        textBuffer->Insert(idxActiveLine, line);
        idxActiveLine++;
    }
    textBuffer->Reparse();

}

void EditController::UpdateSyntaxForBuffer() {
    textBuffer->Reparse();
}


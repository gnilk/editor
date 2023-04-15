//
// Created by gnilk on 15.02.23.
//

#include "EditController.h"

using namespace gedit;

void EditController::Begin() {
    logger = gnilk::Logger::GetLogger("EditorController");
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
    if (DefaultEditLine(cursor, line, keyPress)) {
        return true;
    }
    // Handle backspace in case of cursor.position.x == 0 (i.e. move the line up to the next)
    return false;
}

//
// TODO: Validate - this is more or less lifted from the old EditorMode
//
size_t EditController::NewLine(size_t idxActiveLine, Cursor &cursor) {
    int indentPrevious = 0;
    auto &lines = Lines();
    auto currentLine = LineAt(idxActiveLine);

    //logger->Debug("---> NEW LINE BEGIN");

    if (currentLine != nullptr) {
        // FIXME: the line should not compute the indent - we should search in the parsing meta-data for indent indication..
        indentPrevious = currentLine->ComputeIndent();

//        logger->Debug("Previous Line: %d", idxActiveLine);
//        logger->Debug("Previous: %d:%s-%s:%s", idxActiveLine, lines[idxActiveLine]->startState.c_str(),lines[idxActiveLine]->endState.c_str(), lines[idxActiveLine]->Buffer().data());

    }


    auto it = lines.begin() + idxActiveLine;
    if (lines.size() == 0) {
        it = lines.insert(it, new Line());
    } else {
        if (cursor.position.x == 0) {
//            logger->Debug("New line, previous was empty...");

            // Insert empty line...
            it = lines.insert(it, new Line());
            idxActiveLine++;
            indentPrevious = 0;
        } else {
//            logger->Debug("New line, split at %d", cursor.activeColumn);
            // Split, move some chars from current to new...
            auto newLine = new Line();
            currentLine->Move(newLine, 0, cursor.position.x);

//            logger->Debug("NewLine: 0,%d,%s", cursor.activeColumn, newLine->Buffer().data());
            // TODO: We should invalidate at least X number of lines...
            //UpdateSyntaxForLine(newLine);

            it = lines.insert(it + 1, newLine);
            idxActiveLine++;
        }
    }

    currentLine = lines[idxActiveLine];
    currentLine->SetIndent(indentPrevious);
    int cursorPos = currentLine->Insert(0, indentPrevious, ' ');

    UpdateSyntaxForBuffer();

//    logger->Debug("Active Line: %d", idxActiveLine);
//    logger->Debug("Line StartState: %d:%s:%s", idxActiveLine, lines[idxActiveLine]->startState.c_str(), lines[idxActiveLine]->Buffer().data());
//    logger->Debug("---> NEW LINE DONE");

    cursor.wantedColumn = cursorPos;
    cursor.position.x = cursorPos;
    return idxActiveLine;

}
void EditController::UpdateSyntaxForBuffer() {
    textBuffer->Reparse();
}

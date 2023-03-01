//
// Created by gnilk on 15.02.23.
//

#include "EditController.h"

using namespace gedit;

void EditController::Begin() {
    logger = gnilk::Logger::GetLogger("EditorController");
}

void EditController::SetTextBuffer(TextBuffer *newTextBuffer) {
    textBuffer = newTextBuffer;
    if (onTextBufferChanged != nullptr) {
        onTextBufferChanged();
    }
}


bool EditController::HandleKeyPress(Cursor &cursor, size_t idxLine, const KeyPress &keyPress) {
    if (!textBuffer) {
        return false;
    }
    // FIXME - not quite true, but as long as we don't deal with it yet....
    if (keyPress.modifiers) return  false;

    auto line = textBuffer->LineAt(idxLine);
    if (DefaultEditLine(cursor, line, keyPress)) {
        return true;
    }
    return false;
}

//
// TODO: Validate - this is more or less lifted from the old EditorMode
//
size_t EditController::NewLine(size_t idxActiveLine, Cursor &cursor) {
    int indentPrevious = 0;
    auto &lines = Lines();
    auto currentLine = LineAt(idxActiveLine);

//    logger->Debug("---> NEW LINE BEGIN");

    if (currentLine != nullptr) {
        currentLine->SetActive(false);
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

    lines[idxActiveLine]->SetActive(true);

    UpdateSyntaxForBuffer();

//    logger->Debug("Active Line: %d", idxActiveLine);
//    logger->Debug("Line StartState: %d:%s:%s", idxActiveLine, lines[idxActiveLine]->startState.c_str(), lines[idxActiveLine]->Buffer().data());
//    logger->Debug("---> NEW LINE DONE");

    cursor.wantedColumn = cursorPos;
    cursor.position.x = cursorPos;
    return idxActiveLine;

}
void EditController::UpdateSyntaxForBuffer() {

}

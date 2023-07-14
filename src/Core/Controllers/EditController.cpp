//
// Created by gnilk on 15.02.23.
//
// The controller is in charge of MODIFYING the underlying textbuffer - any such function present in the Model should
// be moved here - see comment in EditorModel.cpp
//

#include "EditController.h"
#include "Core/EditorConfig.h"
#include "Core/UndoHistory.h"
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


bool EditController::HandleKeyPress(Cursor &cursor, size_t &idxLine, const KeyPress &keyPress) {
    if (!textBuffer) {
        return false;
    }
    // Can't just be this blunt - because SHIFT+<number> won't work...
    //if (keyPress.modifiers) return false;

    auto line = textBuffer->LineAt(idxLine);

    auto undoItem = BeginUndoItem(cursor, idxLine);

    if (keyPress.IsHumanReadable()) {
        textBuffer->LangParser().OnPreInsertChar(cursor, line, keyPress.key);
    }
    if (DefaultEditLine(cursor, line, keyPress, false)) {
        if (keyPress.IsHumanReadable()) {
            textBuffer->LangParser().OnPostInsertChar(cursor, line, keyPress.key);
        }
        EndUndoItem(undoItem);
        return true;
    }

    return false;
}

// FIXME: ability to influence active line (this is owned by Model) - currently we have no way to touch this..
bool EditController::HandleSpecialKeyPress(Cursor &cursor, size_t &idxLine, const KeyPress &keyPress) {
    auto line = textBuffer->LineAt(idxLine);

    auto undoItem = BeginUndoItem(cursor, idxLine);

    if (!DefaultEditSpecial(cursor, line, keyPress)) {
        bool wasHandled = false;
        switch (keyPress.specialKey) {
            case Keyboard::kKeyCode_DeleteForward :
                // Handle delete at end of line
                if ((cursor.position.x == line->Length()) && ((idxLine + 1) < textBuffer->NumLines())) {
                    auto next = textBuffer->LineAt(idxLine + 1);
                    line->Append(next);
                    textBuffer->DeleteLineAt(idxLine + 1);
                    wasHandled = true;
                }
                break;
            case Keyboard::kKeyCode_Backspace :
                if ((cursor.position.x == 0) && (idxLine > 0)) {
                    MoveLineUp(cursor, idxLine);
                    wasHandled = true;
                }
                break;
            case Keyboard::kKeyCode_Tab :
                if (keyPress.modifiers == 0) {
                    AddTab(cursor, idxLine);
                } else if (keyPress.IsShiftPressed()) {
                    DelTab(cursor, idxLine);
                }
                wasHandled = true;
                break;


        }
        return wasHandled;
    }
    EndUndoItem(undoItem);
    UpdateSyntaxForBuffer();
    return true;
}

void EditController::MoveLineUp(Cursor &cursor, size_t &idxActiveLine) {
    auto line = textBuffer->LineAt(idxActiveLine);
    auto linePrevious = textBuffer->LineAt((idxActiveLine-1));
    cursor.position.x = linePrevious->Length();
    linePrevious->Append(line);
    textBuffer->DeleteLineAt(idxActiveLine);
    // FIXME: need ability to change activeLine (sits in model)
    idxActiveLine--;
    cursor.position.y--;
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

UndoHistory::UndoItem::Ref EditController::BeginUndoItem(const Cursor &cursor, size_t idxActiveLine) {
    auto undoItem = historyBuffer.NewUndoItem();

    return undoItem;
}

void EditController::EndUndoItem(UndoHistory::UndoItem::Ref undoItem) {
    historyBuffer.PushUndoItem(undoItem);
}


void EditController::AddCharToLineNoUndo(Cursor &cursor, Line::Ref line, int ch) {
    line->Insert(cursor.position.x, ch);
    cursor.position.x++;
    cursor.wantedColumn = cursor.position.x;
}

void EditController::RemoveCharFromLineNoUndo(gedit::Cursor &cursor, Line::Ref line) {
    if (cursor.position.x > 0) {
        line->Delete(cursor.position.x-1);
        cursor.position.x--;
    }
}

void EditController::AddTab(Cursor &cursor, size_t idxActiveLine) {
    auto line = textBuffer->LineAt(idxActiveLine);
    auto undoItem = BeginUndoItem(cursor,idxActiveLine);
    for (int i = 0; i < EditorConfig::Instance().tabSize; i++) {
        AddCharToLineNoUndo(cursor, line, ' ');
    }
    EndUndoItem(undoItem);
}

void EditController::DelTab(Cursor &cursor, size_t idxActiveLine) {
    auto line = textBuffer->LineAt(idxActiveLine);
    auto nDel = EditorConfig::Instance().tabSize;
    if(cursor.position.x < nDel) {
        nDel = cursor.position.x;
    }
    auto undoItem = BeginUndoItem(cursor, idxActiveLine);
    for (int i = 0; i < nDel; i++) {
        RemoveCharFromLineNoUndo(cursor, line);
    }
    EndUndoItem(undoItem);
}

void EditController::AddLineComment(Cursor &cursor, size_t idxLineStart, size_t idxLineEnd, const std::string_view &lineCommentPrefix) {

    // FIXME: Undo for range!!!

    for (size_t idxLine = idxLineStart; idxLine < idxLineEnd; idxLine += 1) {
        auto line = LineAt(idxLine);
        if (!line->StartsWith(lineCommentPrefix)) {
            line->Insert(0, lineCommentPrefix);
        } else {
            line->Delete(0, 2);
        }
    }

    // FIXME: Need 'UpdateSyntaxFromLine(idxLine)'
    UpdateSyntaxForBuffer();
}

// Need cursor for undo...
void EditController::DeleteLines(Cursor &cursor, size_t idxLineStart, size_t idxLineEnd) {
    // Fixme: Need undo for range..
    for(int lineIndex = idxLineStart;lineIndex < idxLineEnd; lineIndex++) {
        // Delete the same line several times - as we move the lines after up..
        textBuffer->DeleteLineAt(idxLineStart);
    }

    UpdateSyntaxForBuffer();
}

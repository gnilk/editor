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
        logger = gnilk::Logger::GetLogger("EditController");
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

    auto line = textBuffer->LineAt(idxLine);
    auto undoItem = BeginUndoItem();

    LanguageBase::kInsertAction parserAction = LanguageBase::kInsertAction::kDefault;

    if (keyPress.IsHumanReadable()) {
        parserAction = textBuffer->LangParser().OnPreInsertChar(cursor, line, keyPress.key);
    }
    // The pre-insert handler for a language can determine if we should 'stop' the default behavior..
    if (parserAction == LanguageBase::kInsertAction::kNoInsert) {
        EndUndoItem(undoItem);
        return true;
    }

    if ((parserAction == LanguageBase::kInsertAction::kDefault) && DefaultEditLine(cursor, line, keyPress, false)) {
        if (keyPress.IsHumanReadable()) {
            textBuffer->LangParser().OnPostInsertChar(cursor, line, keyPress.key);
        }
        EndUndoItem(undoItem);
        return true;
    }

    return false;
}

bool EditController::HandleSpecialKeyPress(Cursor &cursor, size_t &idxLine, const KeyPress &keyPress) {
    auto line = textBuffer->LineAt(idxLine);

    auto undoItem = BeginUndoItem();

    if (!DefaultEditSpecial(cursor, line, keyPress)) {
        bool wasHandled = false;
        switch (keyPress.specialKey) {
            case Keyboard::kKeyCode_DeleteForward :
                // Handle delete at end of line
                if ((cursor.position.x == (int)line->Length()) && ((idxLine + 1) < textBuffer->NumLines())) {
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
    idxActiveLine--;
    cursor.position.y--;
}

void EditController::Undo(Cursor &cursor) {
    if (!historyBuffer.HaveHistory()) {
        return;
    }
    historyBuffer.Dump();
    historyBuffer.RestoreOneItem(cursor, textBuffer);

    UpdateSyntaxForBuffer();
}


size_t EditController::NewLine(size_t idxActiveLine, Cursor &cursor) {
    auto &lines = Lines();
    auto currentLine = LineAt(idxActiveLine);
    auto tabSize = EditorConfig::Instance().tabSize;

    int cursorXPos = 0;

    if (currentLine != nullptr) {
        logger->Debug("NewLine, current=%s [indent=%d]", currentLine->Buffer().data(), currentLine->Indent());
    }

    Line::Ref emptyLine = nullptr;

    auto it = lines.begin() + idxActiveLine;
    if (lines.size() == 0) {
        textBuffer->Insert(it, Line::Create());
        UpdateSyntaxForBuffer();
    } else {
        if (cursor.position.x == 0) {
            // Insert empty line...
            textBuffer->Insert(it, Line::Create());
            UpdateSyntaxForBuffer();
            idxActiveLine++;
        } else {
            // Split, move some chars from current to new...
            auto newLine = Line::Create();
            currentLine->Move(newLine, 0, cursor.position.x);

            // Defer to the language parser if we should auto-insert a new line or not..
            if (textBuffer->LangParser().OnPreCreateNewLine(newLine) == LanguageBase::kInsertAction::kNewLine) {
                // Insert an empty line - this will be the new active line...
                logger->Debug("Creating empty line...");
                emptyLine = Line::Create("");
                textBuffer->Insert(++it, emptyLine);
            }

            textBuffer->Insert(it+1, newLine);

            // This will compute the correct indent, -2/+2 are just arbitary choosen to expand the region
            // clipping is also performed by the syntax parser
            size_t idxStartParse = (idxActiveLine>2)?idxActiveLine-2:0;
            size_t idxEndParse = (textBuffer->NumLines() > (idxActiveLine + 2))?idxActiveLine+2:textBuffer->NumLines();

            UpdateSyntaxForRegion(idxStartParse, idxEndParse);
            WaitForSyntaxCompletion();

            if (emptyLine != nullptr) {
                logger->Debug("EmptyLine, inserting indent: %d", emptyLine->Indent());
                cursorXPos = emptyLine->Insert(0, emptyLine->Indent() * tabSize, ' ');
            }

            auto newX = newLine->Insert(0, currentLine->Indent() * tabSize, ' ');
            // Only assign if not yet done...
            if (cursorXPos == 0) {
                cursorXPos = newX;
                logger->Debug("NewLine, indent=%d, cursorX = %d", newLine->Indent(), cursorXPos);
            }
            idxActiveLine++;
        }
    }


    cursor.wantedColumn = cursorXPos;
    cursor.position.x = cursorXPos;
    return idxActiveLine;
}

// FIX-ME: This needs more info - like the cursor in order to paste the block middle of another block..
void EditController::Paste(size_t idxActiveLine, const char *buffer) {
    std::stringstream strStream(buffer);
    char tmp[GEDIT_MAX_LINE_LENGTH];

    int idxStart = idxActiveLine;
    while(!strStream.eof()) {
        strStream.getline(tmp, GEDIT_MAX_LINE_LENGTH);
        auto line = Line::Create(tmp);
        textBuffer->Insert(idxActiveLine, line);
        idxActiveLine++;
    }
    UpdateSyntaxForRegion(idxStart, idxActiveLine);
}

void EditController::UpdateSyntaxForBuffer() {
    textBuffer->Reparse();
}

void EditController::UpdateSyntaxForRegion(size_t idxStartLine, size_t idxEndLine) {
    textBuffer->ReparseRegion(idxStartLine, idxEndLine);
}

void EditController::WaitForSyntaxCompletion() {
    textBuffer->WaitForParseCompletion();
}


UndoHistory::UndoItem::Ref EditController::BeginUndoItem() {
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
    auto undoItem = BeginUndoItem();
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
    auto undoItem = BeginUndoItem();
    for (int i = 0; i < nDel; i++) {
        RemoveCharFromLineNoUndo(cursor, line);
    }
    EndUndoItem(undoItem);
}

void EditController::AddLineComment(size_t idxLineStart, size_t idxLineEnd, const std::string_view &lineCommentPrefix) {

    auto undoItem = historyBuffer.NewUndoFromLineRange(idxLineStart, idxLineEnd);
    undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kClearAndAppend);
    historyBuffer.PushUndoItem(undoItem);


    for (size_t idxLine = idxLineStart; idxLine < idxLineEnd; idxLine += 1) {
        auto line = LineAt(idxLine);
        if (!line->StartsWith(lineCommentPrefix)) {
            line->Insert(0, lineCommentPrefix);
        } else {
            line->Delete(0, 2);
        }
    }

    UpdateSyntaxForRegion(idxLineStart, idxLineEnd);
}

// Need cursor for undo...
void EditController::DeleteLinesNoSyntaxUpdate(size_t idxLineStart, size_t idxLineEnd) {
    for(auto lineIndex = idxLineStart;lineIndex < idxLineEnd; lineIndex++) {
        // Delete the same line several times - as we move the lines after up..
        textBuffer->DeleteLineAt(idxLineStart);
    }
}

void EditController::DeleteRange(const Point &startPos, const Point &endPos) {
    logger->Debug("DeleteRange, startPos (x=%d, y=%d), endPos (x=%d, y=%d)",
                  startPos.x, startPos.y,
                  endPos.x, endPos.y);

    auto undoItem = historyBuffer.NewUndoFromSelection();
    undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kInsertAsNew);
    historyBuffer.PushUndoItem(undoItem);


    // Delete range within one line..
    if (startPos.y == endPos.y) {
        auto line = textBuffer->LineAt(startPos.y);
        line->Delete(startPos.x, endPos.x - startPos.x);
        return;
    }

    auto startLine = textBuffer->LineAt(startPos.y);
    int y = startPos.y;
    int dy = endPos.y - startPos.y;
    if (startPos.x != 0) {
        startLine->Delete(startPos.x, startLine->Length()-startPos.x);
        y++;
    }
    // If x > 0, we have a partial marked end-line so let's delete that partial data before we chunk the lines
    if (endPos.x > 0) {
        // end-pos is not 0, so we need to chop off stuff at the last line and merge with the first line...
        auto line = textBuffer->LineAt(endPos.y);
        line->Delete(0, endPos.x);
        startLine->Append(line);
    }

    logger->Debug("DeleteRange, fromLine=%d, nLines=%d",y,dy);
    DeleteLinesNoSyntaxUpdate(y, y+dy);

    UpdateSyntaxForRegion(startPos.y, endPos.y+1);

}
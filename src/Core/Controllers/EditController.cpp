//
// Created by gnilk on 15.02.23.
//
// The controller is in charge of MODIFYING the underlying textbuffer - any such function present in the Model should
// be moved here - see comment in EditorModel.cpp
//

#include "EditController.h"
#include "Core/UndoHistory.h"
#include <sstream>
#include "Core/Editor.h"

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

    if (textBuffer->IsReadOnly()) {
        return false;
    }

    auto line = textBuffer->LineAt(idxLine);
    if (line == nullptr) {
        logger->Error("Line is null, idxLine=%zu, cursor=(%d,%d)", idxLine, cursor.position.x, cursor.position.y);
        return false;
    }
    auto undoItem = BeginUndoItem();

    LanguageBase::kInsertAction parserAction = LanguageBase::kInsertAction::kDefault;

    if (keyPress.IsHumanReadable()) {
        parserAction = textBuffer->GetLanguage().OnPreInsertChar(cursor, line, keyPress.key);
    }
    // The pre-insert handler for a language can determine if we should 'stop' the default behavior..
    if (parserAction == LanguageBase::kInsertAction::kNoInsert) {
        EndUndoItem(undoItem);
        return true;
    }

    if ((parserAction == LanguageBase::kInsertAction::kDefault) && DefaultEditLine(cursor, line, keyPress, false)) {
        if (keyPress.IsHumanReadable()) {
            textBuffer->GetLanguage().OnPostInsertChar(cursor, line, keyPress.key);
        }
        EndUndoItem(undoItem);
        UpdateSyntaxForActiveLineRegion(idxLine);
        return true;
    }

    return false;
}

bool EditController::HandleSpecialKeyPress(Cursor &cursor, size_t &idxLine, const KeyPress &keyPress) {
    auto line = textBuffer->LineAt(idxLine);
    auto undoItem = BeginUndoItem();
    bool wasHandled = true;

    if (DefaultEditSpecial(cursor, line, keyPress)) {
        EndUndoItem(undoItem);
    } else {
        // Just drop the undo-item, handle special key must declare it's own...
        wasHandled = HandleSpecialKeyPressForEditor(cursor, idxLine, keyPress);
    }
    UpdateSyntaxForActiveLineRegion(idxLine);
    return wasHandled;
}

bool EditController::HandleSpecialKeyPressForEditor(Cursor &cursor, size_t &idxLine, const KeyPress &keyPress) {
    auto line = textBuffer->LineAt(idxLine);
    bool wasHandled = false;
    switch (keyPress.specialKey) {
        case Keyboard::kKeyCode_DeleteForward :
            // Handle delete at end of line
            if ((cursor.position.x == (int)line->Length()) && ((idxLine + 1) < textBuffer->NumLines())) {
                auto undoItem = historyBuffer.NewUndoFromLineRange(idxLine, idxLine+2);
                undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteFirstBeforeInsert);

                auto next = textBuffer->LineAt(idxLine + 1);
                line->Append(next);
                textBuffer->DeleteLineAt(idxLine + 1);

                EndUndoItem(undoItem);
                wasHandled = true;
            }
            break;
        case Keyboard::kKeyCode_Backspace :
            if ((cursor.position.x == 0) && (idxLine > 0)) {
                auto undoItem = historyBuffer.NewUndoFromLineRange(idxLine-1, idxLine+1);
                undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteFirstBeforeInsert);
                MoveLineUp(cursor, idxLine);
                EndUndoItem(undoItem);
                wasHandled = true;
            }
            break;
        case Keyboard::kKeyCode_Tab :
            {
                auto undoItem = BeginUndoItem();
                if (keyPress.modifiers == 0) {
                    AddTab(cursor, idxLine);
                } else if (keyPress.IsShiftPressed()) {
                    DelTab(cursor, idxLine);
                }
                EndUndoItem(undoItem);
            }

            wasHandled = true;
            break;
    }
    return wasHandled;
}

void EditController::MoveLineUp(Cursor &cursor, size_t &idxActiveLine) {
    auto line = textBuffer->LineAt(idxActiveLine);
    auto linePrevious = textBuffer->LineAt((idxActiveLine-1));

    cursor.wantedColumn = linePrevious->Length();
    linePrevious->Append(line);
    textBuffer->DeleteLineAt(idxActiveLine);
    idxActiveLine--;
    cursor.position.y--;
}

void EditController::Undo(Cursor &cursor, size_t &idxActiveLine) {
    if (!historyBuffer.HaveHistory()) {
        return;
    }
    historyBuffer.Dump();
    logger->Debug("Undo, lines before: %zu", textBuffer->NumLines());
    auto nLinesRestored = historyBuffer.RestoreOneItem(cursor, idxActiveLine, textBuffer);
    logger->Debug("Undo, lines after: %zu", textBuffer->NumLines());

   //UpdateSyntaxForBuffer();
   // UpdateSyntaxForActiveLineRegion(cursor.position.y);
    UpdateSyntaxForRegion(cursor.position.y, cursor.position.y + nLinesRestored * 2);
}

size_t EditController::NewLine(size_t idxActiveLine, Cursor &cursor) {

    auto undoItem = historyBuffer.NewUndoFromLineRange(idxActiveLine, idxActiveLine+1);
    undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteBeforeInsert);


    auto &lines = Lines();
    auto currentLine = LineAt(idxActiveLine);
    //auto tabSize = EditorConfig::Instance().tabSize;
    auto tabSize = textBuffer->GetLanguage().GetTabSize();

    int cursorXPos = 0;

    if (currentLine != nullptr) {
        logger->Debug("NewLine, current=%s [indent=%d]", UnicodeHelper::utf32toascii(currentLine->Buffer().data()).c_str(), currentLine->GetIndent());
    }

    Line::Ref emptyLine = nullptr;

    auto it = lines.begin() + idxActiveLine;
    if (lines.size() == 0) {
        textBuffer->Insert(idxActiveLine, Line::Create());
        UpdateSyntaxForBuffer();
    } else {
        if (cursor.position.x == 0) {
            // Insert empty line...
            textBuffer->Insert(idxActiveLine, Line::Create());
            UpdateSyntaxForActiveLineRegion(idxActiveLine);
            idxActiveLine++;
        } else {
            // Split, move some chars from current to new...
            auto newLine = Line::Create();
            currentLine->Move(newLine, 0, cursor.position.x);

            // Defer to the language parser if we should auto-insert a new line or not..
            if (textBuffer->GetLanguage().OnPreCreateNewLine(newLine) == LanguageBase::kInsertAction::kNewLine) {
                // Insert an empty line - this will be the new active line...
                logger->Debug("Creating empty line...");
                emptyLine = Line::Create(U"");
                textBuffer->Insert(++idxActiveLine, emptyLine);
            }

            textBuffer->Insert(idxActiveLine+1, newLine);

            // This will compute the correct indent, -2/+2 are just arbitary choosen to expand the region
            // clipping is also performed by the syntax parser
            size_t idxStartParse = (idxActiveLine>2)?idxActiveLine-2:0;
            size_t idxEndParse = (textBuffer->NumLines() > (idxActiveLine + 2))?idxActiveLine+2:textBuffer->NumLines();

            auto ptrJob = UpdateSyntaxForRegion(idxStartParse, idxEndParse);
            ptrJob->WaitComplete();

            if (emptyLine != nullptr) {
                logger->Debug("EmptyLine, inserting indent: %d", emptyLine->GetIndent());
                cursorXPos = emptyLine->Indent(tabSize);
                //cursorXPos = emptyLine->Insert(0, emptyLine->Indent() * tabSize, ' ');
            }

            auto newX = tabSize * newLine->Indent(tabSize);
            //auto newX = newLine->Insert(0, currentLine->Indent() * tabSize, ' ');
            // Only assign if not yet done...
            if (cursorXPos == 0) {
                cursorXPos = newX;
                logger->Debug("NewLine, indent=%d, cursorX = %d", newLine->GetIndent(), cursorXPos);
            }
            idxActiveLine++;
        }
    }

    cursor.wantedColumn = cursorXPos;
    cursor.position.x = cursorXPos;

    EndUndoItem(undoItem);

    return idxActiveLine;
}

void EditController::PasteFromClipboard(LineCursor &lineCursor) {
    logger->Debug("Paste from clipboard");
    RuntimeConfig::Instance().GetScreen()->UpdateClipboardData();
    auto &clipboard = Editor::Instance().GetClipBoard();
    if (clipboard.Top() == nullptr) {
        logger->Debug("Clipboard empty!");
        return;
    }
    auto nLines = clipboard.Top()->GetLineCount();
    auto ptWhere = lineCursor.cursor.position;

    auto undoItem = historyBuffer.NewUndoFromLineRange(lineCursor.idxActiveLine, lineCursor.idxActiveLine+nLines);
    undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteBeforeInsert);

    ptWhere.y += (int)lineCursor.viewTopLine;
    clipboard.PasteToBuffer(textBuffer, ptWhere);

    EndUndoItem(undoItem);

    textBuffer->ReparseRegion(lineCursor.idxActiveLine, lineCursor.idxActiveLine + nLines);

    lineCursor.idxActiveLine += nLines;
    lineCursor.cursor.position.y += nLines;
}


void EditController::UpdateSyntaxForBuffer() {
    logger->Debug("Syntax update for full bufffer");
    textBuffer->Reparse();
}

Job::Ref EditController::UpdateSyntaxForRegion(size_t idxStartLine, size_t idxEndLine) {
    logger->Debug("Syntax update for region %zu - %zu", idxStartLine, idxEndLine);
    return textBuffer->ReparseRegion(idxStartLine, idxEndLine);
}

Job::Ref EditController::UpdateSyntaxForActiveLineRegion(size_t idxActiveLine) {
    size_t idxStartParse = (idxActiveLine>2)?idxActiveLine-2:0;
    size_t idxEndParse = (textBuffer->NumLines() > (idxActiveLine + 2))?idxActiveLine+2:textBuffer->NumLines();
    logger->Debug("Syntax update for active line region, active line = %zu", idxActiveLine);
    return UpdateSyntaxForRegion(idxStartParse,idxEndParse);
}


UndoHistory::UndoItem::Ref EditController::BeginUndoItem() {
    auto undoItem = historyBuffer.NewUndoItem();
    return undoItem;
}

void EditController::EndUndoItem(UndoHistory::UndoItem::Ref undoItem) {
    historyBuffer.PushUndoItem(undoItem);
}


void EditController::AddCharToLineNoUndo(Cursor &cursor, Line::Ref line, char32_t ch) {
    line->Insert(cursor.position.x, ch);
    cursor.position.x++;
    cursor.wantedColumn = cursor.position.x;
}

void EditController::RemoveCharFromLineNoUndo(gedit::Cursor &cursor, Line::Ref line) {
    if (cursor.position.x > 0) {
        line->Delete(cursor.position.x-1);
        cursor.position.x--;
        if (cursor.position.x < 0) {
            cursor.position.x = 0;
        }
        cursor.wantedColumn = cursor.position.x;
    }
}

void EditController::AddTab(Cursor &cursor, size_t idxActiveLine) {
    auto line = textBuffer->LineAt(idxActiveLine);
    auto undoItem = BeginUndoItem();

    auto tabSize = textBuffer->GetLanguage().GetTabSize();

    for (int i = 0; i < tabSize; i++) {
        AddCharToLineNoUndo(cursor, line, ' ');
    }
    EndUndoItem(undoItem);
}

void EditController::DelTab(Cursor &cursor, size_t idxActiveLine) {
    auto line = textBuffer->LineAt(idxActiveLine);
    auto nDel = textBuffer->GetLanguage().GetTabSize();
    if(cursor.position.x < nDel) {
        nDel = cursor.position.x;
    }
    auto undoItem = BeginUndoItem();
    for (int i = 0; i < nDel; i++) {
        RemoveCharFromLineNoUndo(cursor, line);
    }
    EndUndoItem(undoItem);
}

void EditController::AddLineComment(size_t idxLineStart, size_t idxLineEnd, const std::u32string &lineCommentPrefix) {

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

void EditController::IndentLines(size_t idxLineStart, size_t idxLineEnd) {
    auto undoItem = historyBuffer.NewUndoFromLineRange(idxLineStart, idxLineEnd);
    undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kClearAndAppend);
    historyBuffer.PushUndoItem(undoItem);

    auto tabSize = GetTextBuffer()->GetLanguage().GetTabSize();
    std::u32string strIndent;
    for(int i=0;i<tabSize;i++) {
        strIndent += U" ";
    }

    for (size_t idxLine = idxLineStart; idxLine < idxLineEnd; idxLine += 1) {
        auto line = LineAt(idxLine);
        line->Insert(0, strIndent);
    }

    UpdateSyntaxForRegion(idxLineStart, idxLineEnd);
}
void EditController::UnindentLines(size_t idxLineStart, size_t idxLineEnd) {
    auto undoItem = historyBuffer.NewUndoFromLineRange(idxLineStart, idxLineEnd);
    undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kClearAndAppend);
    historyBuffer.PushUndoItem(undoItem);

    auto tabSize = GetTextBuffer()->GetLanguage().GetTabSize();

    for (size_t idxLine = idxLineStart; idxLine < idxLineEnd; idxLine += 1) {
        auto line = LineAt(idxLine);
        line->Unindent(tabSize);
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
    if ((startPos.x == 0) && (endPos.x == 0)) {
        undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kInsertAsNew);
    } else {
        undoItem->SetRestoreAction(UndoHistory::kRestoreAction::kDeleteFirstBeforeInsert);
    }
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
    // FIX-ME: Special case, when (endPos.x == 0) && (start.x > 0) && (start.y != end.y) -> we should pull the last FULL line upp to start.x
    // Perhaps easier, if startPos.x > 0 and start.y != end.y we should concat the endpos line
    // I.e. no need for the if-case below, it can be integrated in to the upper if-case and solved directly (which makes it easier)

    // If x > 0, we have a partial marked end-line so let's delete that partial data before we chunk the lines
    if (endPos.x > 0) {
        // end-pos is not 0, so we need to chop off stuff at the last line and merge with the first line...
        auto line = textBuffer->LineAt(endPos.y);
        line->Delete(0, endPos.x);
        startLine->Append(line);
    }

    logger->Debug("DeleteRange, fromLine=%d, nLines=%d",y,dy);
    if (dy > 0) {
        DeleteLinesNoSyntaxUpdate(y, y + dy);
    }

    UpdateSyntaxForRegion(startPos.y, endPos.y+1);

}
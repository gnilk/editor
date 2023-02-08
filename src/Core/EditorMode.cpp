//
// Created by gnilk on 14.01.23.
//

// tmp
#include <ncurses.h>
// end-tmp

#include <utility>
#include "Core/Line.h"
#include "Core/EditorMode.h"
#include "Core/KeyCodes.h"
#include "Core/RuntimeConfig.h"

EditorMode::EditorMode() {
}
bool EditorMode::Begin() {
    logger = gnilk::Logger::GetLogger("EditorMode");

    buffer = new Buffer();
    NewLine();
    idxActiveLine = 0;

    return true;
}

void EditorMode::SetBuffer(Buffer *newBuffer) {
    buffer = newBuffer;
    currentLine = buffer->LineAt(0);
    currentLine->SetActive(true);
    idxActiveLine = 0;
}

void EditorMode::NewLine() {
    int indentPrevious = 0;
    auto &lines = buffer->Lines();

//    logger->Debug("---> NEW LINE BEGIN");

    if (currentLine != nullptr) {
        currentLine->SetActive(false);
        // FIXME: the line should not compute the indent - we should search in the parsing meta-data for indent indication..
        indentPrevious = currentLine->ComputeIndent();

//        logger->Debug("Previous Line: %d", idxActiveLine);
//        logger->Debug("Previous: %d:%s-%s:%s", idxActiveLine, lines[idxActiveLine]->startState.c_str(),lines[idxActiveLine]->endState.c_str(), lines[idxActiveLine]->Buffer().data());

    }


    auto it = lines.begin() + idxActiveLine;
    if (buffer->Lines().size() == 0) {
        it = lines.insert(it, new Line());
    } else {
        if (cursor.activeColumn == 0) {
//            logger->Debug("New line, previous was empty...");

            // Insert empty line...
            it = lines.insert(it, new Line());
            idxActiveLine++;
            indentPrevious = 0;
        } else {
//            logger->Debug("New line, split at %d", cursor.activeColumn);
            // Split, move some chars from current to new...
            auto newLine = new Line();
            currentLine->Move(newLine, 0, cursor.activeColumn);

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

    cursor.activeColumn = cursorPos;
}
void EditorMode::ClearSelectedLines() {
    // FIXME: We don't want to clear all, once we have a proper selection structure we should
    //        keep delta and clear only what is needed
    for(auto &line : buffer->Lines()) {
        line->SetSelected(false);
    }
}


void EditorMode::DrawLines() {
    auto screen = RuntimeConfig::Instance().Screen();
    auto &lines = buffer->Lines();

    if (selection.IsActive()) {

        ClearSelectedLines();
        screen->InvalidateAll();

        int idxStart = selection.idxStartLine;
        int idxEnd = selection.idxEndLine;
        if (idxStart > idxEnd) {
            std::swap(idxStart, idxEnd);
        }
        for(int i=idxStart;i<idxEnd;i++) {
            lines[i]->SetSelected(true);
        }
    }

    screen->DrawGutter(idxActiveLine);
    screen->SetCursorColumn(cursor.activeColumn);
    screen->DrawLines(Lines(),idxActiveLine);

    // FIXME: Status bar should have '<buffer>:<filename> | <type> | <indent size> | ..  perhaps..
    // like: 0:config.yml

    // Bottom bar: status
    auto indent = currentLine->Indent();
    char tmp[256];
    snprintf(tmp, 256, "Goat Editor v0.1 - lc: %d (%s)- al: %d - ts: %d - s: %s (%d - %d)",
             (int)lastChar.data.code, keyname((int)lastChar.rawCode), idxActiveLine, indent,
             selection.IsActive()?"y":"n", selection.idxStartLine, selection.idxEndLine);
    // TODO: Pad this to end of screen
    screen->DrawBottomBar(tmp);


    // Top bar: Active buffers bar
    screen->DrawTopBar("1:file.txt | 2:main.cpp | 3:readme.md | 4:CMakeLists.txt | 5:dummy.xyz");
}

void EditorMode::UpdateSyntaxForCurrentLine() {
    UpdateSyntaxForLine(currentLine);
}

void EditorMode::UpdateSyntaxForLine(Line *line) {
    auto &tokenizer = buffer->LangParser().Tokenizer();

    if (!line->startState.empty()) {
        tokenizer.ParseLineFromStartState(line->startState, line);
//        logger->Debug("Update Syntax: %d:%s:%s-%s:Attribs:%d", idxActiveLine,line->Buffer().data(),line->startState.c_str(), line->endState.c_str(),(int)line->Attributes().size());
//        for(auto a : line->Attributes()) {
//            logger->Debug(" %d:%d",a.idxOrigString,a.idxColor);
//        }

    } else {
        std::vector<gnilk::LangToken> tokens;
        tokenizer.ParseLine(tokens, line->Buffer().data());
        gnilk::LangToken::ToLineAttrib(line->Attributes(), tokens);
    }
}

void EditorMode::UpdateSyntaxForBuffer() {
    buffer->Reparse();
}


void EditorMode::Update() {
    auto kbd = RuntimeConfig::Instance().Keyboard();

    auto keyPress = kbd->GetCh();

    //auto ch = getch();
    lastChar = keyPress;
    if (!keyPress.IsValid()) {
        return;
    }

    if (keyPress.rawCode != -1) {
        logger->Debug("Key, raw=0x%.2x (special=0x%.2x, code=0x%.2x)", keyPress.rawCode, keyPress.data.special,
                      keyPress.data.code);
    }


    if (DefaultEditLine(currentLine, keyPress)) {
        if (keyPress.IsHumanReadable()) {
            // TODO: Check language features here
            // Like:
            //  if '}' was entered as first char on a line - unindent it
            //  if '{' was entered add '}'
        }
        UpdateSyntaxForCurrentLine();
        return;
    }

    // Update any navigation related, including selection handling (as it relates to navigation)
    if (UpdateNavigation(keyPress, keyPress.IsShiftPressed())) {
        return;
    }
    // Do other things here...
}

//
// Returns true if the keypress was handled
//
bool EditorMode::UpdateNavigation(KeyPress &keyPress, bool isShiftPressed) {

    auto screen = RuntimeConfig::Instance().Screen();
    auto [rows, cols] = screen->Dimensions();

    // save current line - as it will update with navigation
    // we need it when we update the selection status...
    auto idxLineBeforeNavigation = idxActiveLine;

    switch (keyPress.data.code) {
        case kKey_Down :
            OnNavigateDown(1);
            cursor.activeColumn = cursor.wantedColumn;
            if (cursor.activeColumn > currentLine->Length()) {
                cursor.activeColumn = currentLine->Length();
            }
            break;
        case kKey_Up :
            OnNavigateUp(1);
            cursor.activeColumn = cursor.wantedColumn;
            if (cursor.activeColumn > currentLine->Length()) {
                cursor.activeColumn = currentLine->Length();
            }
            break;
        case kKey_PageUp :
            OnNavigateUp(rows-2);
            break;
        case kKey_PageDown :
            OnNavigateDown(rows-2);
            break;
        case kKey_Return :
            NewLine();
            screen->InvalidateAll();
            break;
        default:
            // Not navigation
            return false;
    }

    // Do selection handling
    if (isShiftPressed) {
        if (!selection.IsActive()) {
            selection.Begin(idxLineBeforeNavigation);
        }
        selection.Continue(idxActiveLine);
    } else if (selection.IsActive()) {
        selection.SetActive(false);
        ClearSelectedLines();
        screen->InvalidateAll();
    }

    return true;

}
void EditorMode::OnNavigateDown(int rows) {
    currentLine->SetActive(false);
    auto &lines = buffer->Lines();

    idxActiveLine+=rows;
    if (idxActiveLine >= lines.size()) {
        idxActiveLine = lines.size()-1;
    }

    currentLine = lines[idxActiveLine];
    currentLine->SetActive(true);
}

void EditorMode::OnNavigateUp(int rows) {
    currentLine->SetActive(false);
    auto &lines = buffer->Lines();

    idxActiveLine -= rows;
    if (idxActiveLine < 0) {
        idxActiveLine = 0;
    }

    currentLine = lines[idxActiveLine];
    currentLine->SetActive(true);
}

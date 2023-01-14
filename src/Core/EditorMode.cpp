//
// Created by gnilk on 14.01.23.
//

// TMP
#include <ncurses.h>
#include "Core/NCurses/NCursesScreen.h"
// END-TMP

#include "Core/Line.h"
#include "Core/EditorMode.h"
#include "Core/KeyCodes.h"

EditorMode::EditorMode() {
    NewLine();
    idxActiveLine = 0;
}

void EditorMode::SetBuffer(Buffer &newBuffer) {
    lines = std::move(newBuffer);
    currentLine = lines[0];
    idxActiveLine = 0;
}

void EditorMode::NewLine() {
    int indentPrevious = 0;
    if (currentLine != nullptr) {
        currentLine->SetActive(false);
        indentPrevious = currentLine->ComputeIndent();
    }

    auto it = lines.begin() + idxActiveLine;
    if (lines.size() == 0) {
        it = lines.insert(it, new Line());
    } else {
        if (cursor.activeColumn == 0) {
            // Insert empty line...
            it = lines.insert(it, new Line());
            idxActiveLine++;
            indentPrevious = 0;
        } else {
            // Split, move some chars from current to new...
            auto newLine = new Line();
            currentLine->Move(newLine, 0, cursor.activeColumn);
            it = lines.insert(it + 1, newLine);
            idxActiveLine++;
        }
    }

    currentLine = lines[idxActiveLine];
    currentLine->SetIndent(indentPrevious);
    int cursorPos = currentLine->Insert(0, indentPrevious, ' ');

    lines[idxActiveLine]->SetActive(true);

    cursor.activeColumn = cursorPos;
}

void EditorMode::DrawLines(NCursesScreen &screen) {
    screen.DrawGutter(idxActiveLine);
    screen.SetCursorColumn(cursor.activeColumn);
    screen.DrawLines(Lines(),idxActiveLine);

    auto indent = currentLine->Indent();
    char tmp[256];
    snprintf(tmp, 256, "Goat Editor v0.1 - lc: %d (%s)- al: %d - ts: %d - ", lastChar, keyname(lastChar), idxActiveLine, indent);
    screen.DrawStatusBar(tmp);
}


void EditorMode::Update(NCursesScreen &screen) {
    auto ch = getch();
    lastChar = ch;
    if (ch == ERR) {
        return;
    }

    auto [rows, cols] = screen.Dimensions();

    if (DefaultEditLine(currentLine, ch)) {
        if ((ch > 31) && (ch < 127)) {
            // TODO: Check language features here
            // Like:
            //  if '}' was entered as first char on a line - unindent it
            //  if '{' was entered add '}'
        }
        return;
    }

    switch (ch) {
        case KEY_DOWN :
            OnNavigateDown(1);
            cursor.activeColumn = cursor.wantedColumn;
            if (cursor.activeColumn > currentLine->Length()) {
                cursor.activeColumn = currentLine->Length();
            }
            break;
        case KEY_UP :
            OnNavigateUp(1);
            cursor.activeColumn = cursor.wantedColumn;
            if (cursor.activeColumn > currentLine->Length()) {
                cursor.activeColumn = currentLine->Length();
            }
            break;
        case KEY_PPAGE :
            OnNavigateUp(rows-1);
            break;
        case KEY_NPAGE :
            OnNavigateDown(rows-1);
            break;
        case kKey_Return :
            NewLine();
            screen.InvalidateAll();
            break;
    }
}
void EditorMode::OnNavigateDown(int rows) {
    currentLine->SetActive(false);

    idxActiveLine+=rows;
    if (idxActiveLine >= lines.size()) {
        idxActiveLine = lines.size()-1;
    }

    currentLine = lines[idxActiveLine];
    currentLine->SetActive(true);
}

void EditorMode::OnNavigateUp(int rows) {
    currentLine->SetActive(false);

    idxActiveLine -= rows;
    if (idxActiveLine < 0) {
        idxActiveLine = 0;
    }

    currentLine = lines[idxActiveLine];
    currentLine->SetActive(true);
}

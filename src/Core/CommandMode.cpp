//
// Created by gnilk on 14.01.23.
//

// TMP
#include <ncurses.h>

/*
 * Command Mode is where you sort of have a shell with an interpretator sitting before it..
 * unless it is a known command it will be sent to the shell..
 * note: perhaps we need to prefix with '.<cmd>' for internal stuff unless it is a special character?
 *
 * a cmdlet behave like a shell-command and have the following:
 * - long name, like "open_file"
 * - short name, ".o"
 * - description, "open_file <filename>" (this can be a multiline)
 *
 * The command parser will check if the first token is a short/long cmdlet name if so, it will execute it
 * otherwise the whole line will be sent down to the shell...
 *
 * There will be some decoupling and an internal API made available to the cmdlet. This will allow
 * cmdlet's to be placed in dylib's.
 *
 */


#include "Core/ScreenBase.h"
#include "Core/CommandMode.h"
#include "Core/Line.h"
#include "Core/KeyCodes.h"

CommandMode::CommandMode() {
    NewLine();
}

void CommandMode::NewLine() {
    if (currentLine != nullptr) {
        currentLine->SetActive(false);
    }
    currentLine = new Line();
    currentLine->Append('>');
    cursor.activeColumn = 1;
    currentLine->SetActive(true);
    historyBuffer.push_back(currentLine);

}

void CommandMode::DrawLines(ScreenBase &screen) {
    screen.SetCursorColumn(cursor.activeColumn);
    screen.DrawLines(Lines(),0);
}
void CommandMode::Update(ScreenBase &screen) {
    auto ch = getch();
    if (ch == ERR) {
        return;
    }

    if (DefaultEditLine(currentLine, ch)) {
        return;
    }

    switch(ch) {
        case kKey_Return :
            // Proper handling here!
            // Here we should parse the buffer and map to the command list..
            // like:
            //  'o <filename>' for 'open file'
            //  's' - save
            //  's <filename>'
            //  '? <expr>' resolve expression
            //  'make' run make
            //  'cd' change current working directory
            // .....

            if ((currentLine->Buffer() == ">quit") || (currentLine->Buffer() == ">.q")) {
                onExitApp();
                return;
            }
            NewLine();
            // Execute..
            break;
        case kKey_Escape :
            // toogle into terminal mode...
            if (onExitMode != nullptr) {
                onExitMode();
            }
            break;
    }
}

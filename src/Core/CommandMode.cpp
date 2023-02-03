//
// Created by gnilk on 14.01.23.
//
// TODO:
// ! Replace Process.cpp with: https://dev.to/aggsol/calling-shell-commands-from-c-8ej
// - Create a shell thread directly, use stdin to feed it
//    i.e. if argument is a shell cmd pass it directly to the running shell thread
// TMP
#include <ncurses.h>

#include "Core/Config/Config.h"

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
#include "Core/RuntimeConfig.h"
#include "Core/StrUtil.h"
#include <pthread.h>
#include <thread>
#include <sstream>


CommandMode::CommandMode() {
    for(int i=0;i<50;i++) {
        NewLine();
        char tmp[64];
        snprintf(tmp, 64, "this is line %d", i);
        currentLine->Append(tmp);

    }
    NewLine();
}

bool CommandMode::Begin() {
    terminal.SetStdoutDelegate([this](std::string &output) {
        // TODO: Remove marker on current line...

        currentLine->Append(output);
        fprintf(log, "GOT: %s\n", output.c_str());

        NewLine(true);

        if (isModeActive) {
            auto screen = RuntimeConfig::Instance().Screen();
            screen->Scroll(1);
        }

        fflush(log);
    });

    if (!terminal.Begin()) {
        return false;
    }

    logger = gnilk::Logger::GetLogger("CommandMode");
    auto prompt = Config::Instance()["commandmode"].GetStr("prompt");

    SetColumnOffset(prompt.length());


    log = fopen("log.txt", "w+");
    fprintf(log, "test\n");
    return true;
}

void CommandMode::OnSwitchMode(bool enter) {
    isModeActive = enter;

    if (enter) {
        scrollOnNextUpdate = true;
    }
}


void CommandMode::NewLine(bool addCmdMarker) {
    if (currentLine != nullptr) {
        currentLine->SetActive(false);
    }
    std::lock_guard<std::mutex> guard(lineLock);
    currentLine = new Line();
    cursor.activeColumn = columnOffset;
    currentLine->SetActive(true);
    historyBuffer.push_back(currentLine);
}

void CommandMode::DrawLines() {
    auto screen = RuntimeConfig::Instance().Screen();
    auto [rows, cols] = screen->Dimensions();

    auto prompt = Config::Instance()["commandmode"].GetStr("prompt");

    screen->NoGutter();

    // When changing modes it is nice to contain some part of the editor screen
    // So we don't swap out the full editor but rather contain at least half
    if (scrollOnNextUpdate) {
        int nLinesToScroll = historyBuffer.size();
        // Cut off at half of the rows of the screen...
        if (historyBuffer.size() > rows/2) {
            nLinesToScroll = rows/2;
        }
        screen->Scroll(nLinesToScroll);
        scrollOnNextUpdate = false;
    }

    screen->SetCursorColumn(cursor.activeColumn);

    // Print backwards, but only show part of our history buffer...
    int nHistoryLines = historyBuffer.size();
    if (nHistoryLines > rows/2) {
        nHistoryLines = rows/2;
    }
    for(int i=0;i<nHistoryLines;i++) {
        screen->DrawLineAt(rows-i-1, prompt, historyBuffer[historyBuffer.size() - i - 1]);
    }
    screen->DrawLineAt(rows -1, prompt, currentLine);
}
//
// Update data - this is called before draw
// We process input here!
//
void CommandMode::Update() {

    auto kbd = RuntimeConfig::Instance().Keyboard();
    auto screen = RuntimeConfig::Instance().Screen();

    // FIXME: Splt in two depending on state
    // Don't accept input if we are executing shell command
//    if (state == kState::kExecuteShell) {
//        if (shCommand.IsDone()) {
//            state = kState::kIdle;
//            NewLine();
//            screen->Scroll(1);
//
//        }
//        return;
//    }

    auto keyPress = kbd->GetCh();
    if (!keyPress.IsValid()) {
        return;
    }

    if (DefaultEditLine(currentLine, keyPress)) {
        return;
    }

    switch(keyPress.data.code) {
        case kKey_Return :
            HandleReturn();
            NewLine();
            screen->Scroll(1);
            break;
        case kKey_Escape :
            // toogle into terminal mode...
            if (onExitMode != nullptr) {
                onExitMode();
            }
            break;
            // TEST TEST
        case kKey_Down :
            scroll(stdscr);
            break;
    }
}

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
void CommandMode::HandleReturn() {
    std::string cmdLine(currentLine->Buffer().data());


    if ((cmdLine == "quit") || (cmdLine == ".q")) {
        fclose(log);
        onExitApp();
        return;
    }
    // Just push this to the shell "process"...
    if (cmdLine.size() < 1) {
        return;
    }

    strutil::trim(cmdLine);
    logger->Debug("ExecuteShell: %s", currentLine->Buffer().data());
    fprintf(log, "%s\n", currentLine->Buffer().data());
    cmdLine += "\n";
    terminal.SendCmd(cmdLine);
}

void CommandMode::TestExecuteShellCmd() {
    Shell sh;
    sh.SetStdoutDelegate([](std::string &str) {
        printf("%s\n",str.c_str());
    });
    sh.Begin();

    char buffer[256];
    while(true) {
        auto res = fgets(buffer, 256, stdin);
        if (res == nullptr) {
            continue;
        }
        std::string strCommand(buffer);
        sh.SendCmd(strCommand);
        std::this_thread::yield();
    }
}


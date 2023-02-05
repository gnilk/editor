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
#include "logger.h"
#include <unordered_map>
#include <pthread.h>
#include <thread>
#include <sstream>

// TODO: Define some simple API stuff (WriteLine/Printf, etc..)
//       And define the commands we need
struct EditorCmdLetAPI {
    void Quit() {
        int breakme;
        breakme = 1;
        exit(1);
    }
};

class CmdLet {
public:
    CmdLet() = default;
    // Note: not quite sure yet what to do with the result...
    virtual const std::string &GetShortName() const { return emptyName; };
    virtual bool Execute(std::vector<std::string_view> &args) { return false; };
public:
    // TODO: Fix proxy functions here...
protected:
    EditorCmdLetAPI *api;
private:
    static std::string emptyName;
};
std::string CmdLet::emptyName("");

class CommandRepository {
public:
    static bool HasCommand(const std::string &name) {
        if (commands.find(name) != commands.end()) {
            return true;
        }
        if (shortNameCommands.find(name) != shortNameCommands.end()) {
            return true;
        }
        return false;
    }
    static void RegisterCommand(const std::string &name, CmdLet *cmdlet) {
        auto logger = gnilk::Logger::GetLogger("CmdRepo");
        logger->Debug("Register cmdlet '%s', short: '%s'", name.c_str(), cmdlet->GetShortName().c_str());

        commands[name] = cmdlet;
        auto shortName = cmdlet->GetShortName();
        if (shortName.empty()) {
            return;
        }
        shortNameCommands[shortName] = cmdlet;
    }
    static CmdLet *GetCommand(const std::string &name) {
        if (commands.find(name) != commands.end()) {
            return commands[name];
        }
        if (shortNameCommands.find(name) != shortNameCommands.end()) {
            return shortNameCommands[name];
        }
        return nullptr;
    }
private:
    static std::unordered_map<std::string, CmdLet *> commands;
    static std::unordered_map<std::string, CmdLet *> shortNameCommands;
};

std::unordered_map<std::string, CmdLet *> CommandRepository::commands;
std::unordered_map<std::string, CmdLet *> CommandRepository::shortNameCommands;

//
// this will exit no matter what..
//
class CmdQuit : public CmdLet {
public:
    const std::string &GetShortName() const override {
        static std::string shortName="q!";
        return shortName;
    }
    bool Execute(std::vector<std::string_view> &args) override {
        // TODO: Need an API here..
        api->Quit();
        return true;
    }
};
class CmdQuitAndSave : public CmdLet {
public:
    const std::string &GetShortName() const override {
        static std::string shortName="qs";
        return shortName;
    }
    bool Execute(std::vector<std::string_view> &args) override {
        // TODO: Need an API here..
        api->Quit();
        return true;
    }
};

static std::vector<std::pair<std::string, CmdLet *>> builtInCmds = {
        {"quit!", new CmdQuit()},
        {"quit", new CmdQuitAndSave()},
};


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
    logger = gnilk::Logger::GetLogger("CommandMode");
    logger->Debug("Begin");


    for(auto &cmd : builtInCmds) {
        CommandRepository::RegisterCommand(cmd.first, cmd.second);
    }
    ResetVariablesFromConfig();


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


    SetColumnOffset(cmdPrompt.length());



    // Use a factory/callback thingie here..
    // like: dload("file.dylib");
    // factory = dlsym(factory);
    // factory([this](const std::string &name, CmdLet *cmdLet) {
    //    CommandRepository::RegisterCommand(name, cmdLet);
    // });
    // Register all built in commands..


    log = fopen("log.txt", "w+");
    fprintf(log, "test\n");
    return true;
}
void CommandMode::ResetVariablesFromConfig() {
    cmdletPrefix = Config::Instance()["commandmode"].GetChar("cmdlet_prefix",'.');
    cmdPrompt = Config::Instance()["commandmode"].GetStr("prompt",">");
    logger->Debug("Config load/reload");
    logger->Debug("prompt       : %s",cmdPrompt.c_str());
    logger->Debug("cmdlet prefix: %c",cmdletPrefix);
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
        screen->DrawLineAt(rows-i-1, cmdPrompt, historyBuffer[historyBuffer.size() - i - 1]);
    }
    screen->DrawLineAt(rows -1, cmdPrompt, currentLine);
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
    if (cmdLine.size() < 1) {
        return;
    }
    // Make this configurable?? - yes of course - we want all of the editor to be configurable...  =)
    if (cmdLine[0] == cmdletPrefix && (cmdLine.size() > 1)) {
        // remove 'bang' char
        cmdLine.erase(0,1);
        auto cmd = CommandRepository::GetCommand(cmdLine);
        if (cmd != nullptr) {
            std::vector<std::string_view> args = {};
            cmd->Execute(args);
        }
    }

    // TEMPORARY - this should be built in...
//    if ((cmdLine == "quit") || (cmdLine == ".q")) {
//        fclose(log);
//        onExitApp();
//        return;
//    }
    if (cmdLine.size() < 1) {
        return;
    }
    // Just push this to the shell "process"...

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


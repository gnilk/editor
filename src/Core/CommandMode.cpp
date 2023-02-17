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
#include "Core/HexDump.h"
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

    void WriteLine(const std::string &str) {
        auto console = RuntimeConfig::Instance().OutputConsole();
        console->WriteLine(str);
    }
};

class CmdLet {
public:
    CmdLet() = default;
    // Note: not quite sure yet what to do with the result...
    virtual const std::string &GetShortName() const { return emptyName; };
    virtual bool Execute(std::vector<std::string> &args) { return false; };
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
    bool Execute(std::vector<std::string> &args) override {
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
    bool Execute(std::vector<std::string> &args) override {
        // TODO: Need an API here..
        api->Quit();
        return true;
    }
};
class CmdEcho : public CmdLet {
public:
    const std::string &GetShortName() const override {
        static std::string shortName = "ec";
        return shortName;
    }
    bool Execute(std::vector<std::string> &args) override {
        std::string concat;
        for(size_t i=1;i<args.size();i++) {
            concat += args[i] + ' ';
        }
        api->WriteLine(concat);
        return true;
    }
};

class CmdGenStrings : public CmdLet {
public:
    const std::string &GetShortName() const override {
        static std::string shortName = "gs";
        return shortName;
    }
    bool Execute(std::vector<std::string> &args) override {
        for(int i=0;i<10;i++) {
            char buffer[128];
            snprintf(buffer, 128, "== %d Generated %d ==",lc,lc);
            api->WriteLine(buffer);
            lc++;
        }
        return true;
    }
private:
    int lc = 0;
};

static std::vector<std::pair<std::string, CmdLet *>> builtInCmds = {
        {"quit!", new CmdQuit()},
        {"quit", new CmdQuitAndSave()},
        {"echo", new CmdEcho()},
        {"genstrings", new CmdGenStrings()},
};

CommandMode::CommandMode() {
    NewLine();
}

bool CommandMode::Begin() {
    logger = gnilk::Logger::GetLogger("CommandMode");
    logger->Debug("Begin");


    for(auto &cmd : builtInCmds) {
        CommandRepository::RegisterCommand(cmd.first, cmd.second);
    }
    ResetVariablesFromConfig();

    // Terminal sends any output from the thread through this, so we hook this up to the writeline function
    terminal.SetStdoutDelegate([this](std::string &output) {
        WriteLine(output);
    });

    // Now we start the terminal, this will fork a shell process and hook up stdin/stdout properly
    if (!terminal.Begin()) {
        return false;
    }


    // Use a factory/callback thingie here..
    // like: dload("file.dylib");
    // factory = dlsym(factory);
    // factory([this](const std::string &name, CmdLet *cmdLet) {
    //    CommandRepository::RegisterCommand(name, cmdLet);
    // });
    // Register all built in commands..

    return true;
}

//
// Write a line, this will essentially just append on the current line, create a new line and scroll (if active)
//
void CommandMode::WriteLine(const std::string &str) {
    currentLine->Append(str);
    NewLine(true);
    if (isModeActive) {
        auto screen = RuntimeConfig::Instance().Screen();
        screen->Scroll(1);
    }
}

//
// Read out variables from config with defaults if not present..
//
void CommandMode::ResetVariablesFromConfig() {
    cmdletPrefix = Config::Instance()["commandmode"].GetChar("cmdlet_prefix",'.');
    cmdPrompt = Config::Instance()["commandmode"].GetStr("prompt",">");
    SetColumnOffset(cmdPrompt.length());

    logger->Debug("Config load/reload");
    logger->Debug("prompt       : %s",cmdPrompt.c_str());
    logger->Debug("cmdlet prefix: %c",cmdletPrefix);
    logger->Debug("Column Offset: %d",columnOffset);
}

//
// When switching modes we will push history buffer and scroll on next update...
//
void CommandMode::OnSwitchMode(bool enter) {
    isModeActive = enter;

    if (enter) {
        scrollOnNextUpdate = true;
    }
}

//
// Create a new line and push to the history buffer...
//
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

//
// Draw our lines...
//
void CommandMode::DrawLines() {
    auto screen = RuntimeConfig::Instance().Screen();
    auto view = RuntimeConfig::Instance().View();
    auto dimensions = view->ViewRect();


    // NOTE: we should probably have a setting for a 'safe' zone
    int maxRowsToScroll = dimensions.Height();

    screen->NoGutter();

    // When changing modes it is nice to contain some part of the editor screen
    // So we don't swap out the full editor but rather contain at least half
    if (scrollOnNextUpdate) {
        int nLinesToScroll = historyBuffer.size();
        // Cut off at half of the rows of the screen...
        if (historyBuffer.size() > maxRowsToScroll) {
            nLinesToScroll = maxRowsToScroll;
        }
        // FIXME: this request should go to the view and not the screen!!!
        screen->Scroll(nLinesToScroll);
        scrollOnNextUpdate = false;
    }

    screen->SetCursorColumn(cursor.activeColumn);

    // Print backwards, but only show part of our history buffer...
    int nHistoryLines = historyBuffer.size();
    if (nHistoryLines > maxRowsToScroll) {
        nHistoryLines = maxRowsToScroll;
    }
    for(int i=0;i<nHistoryLines;i++) {
        screen->DrawLineAt(dimensions.Height()-i-1, cmdPrompt, historyBuffer[historyBuffer.size() - i - 1]);
    }
    screen->DrawLineAt(dimensions.Height()-1, cmdPrompt, currentLine);
}
//
// Update data - this is called before draw
// We process input here!
//
void CommandMode::Update() {

    auto kbd = RuntimeConfig::Instance().Keyboard();

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

//
// Handle a new committed line (i..e after return)
//
void CommandMode::HandleReturn() {
    std::string cmdLine(currentLine->Buffer().data());
    if (cmdLine.size() < 1) {
        return;
    }
    if (TryExecuteInternalCmd(cmdLine)) {
        return;
    }
    TryExecuteShellCmd(cmdLine);
}

//
// Try and execute the cmd line as an internal command
// Note: Internal command must be properly prefixed ('config.commandmode.cmdlet_prefix')
//
bool CommandMode::TryExecuteInternalCmd(std::string &cmdline) {
    auto screen = RuntimeConfig::Instance().Screen();

    if (!DoesCmdLineHavePrefix(cmdline)) {
        return false;
    }

    // check if we start with prefix - in that case we check if an internal command first before passing it on...
    std::vector<std::string> args;

    // Split string to arg-list and grab first item as the internal command
    strutil::splitToStringList(args, strutil::trim(cmdline).c_str());
    std::string internalCmd(args[0],1);

    auto cmd = CommandRepository::GetCommand(internalCmd);
    if (cmd != nullptr) {
        logger->Debug("Executing internal cmd: '%s'", internalCmd.c_str());
        // Need to scroll here as well otherwise the 'STATUS' bar from edit-mode is deleted..
        NewLine(true);
        screen->Scroll(1);
        return cmd->Execute(args);
    } else {
        logger->Error("InternalCmd: '%s' not found", internalCmd.c_str());
    }
    return true;
}

//
// Execute through the shell - this essentially just writes the cmdline out to stdin for the shell thread...
//
void CommandMode::TryExecuteShellCmd(std::string &cmdline) {
    // Just push this to the shell "process"...
    strutil::trim(cmdline);
    logger->Debug("Trying shell: %s", cmdline.c_str());

    // Make sure to append CRLN otherwise the shell won't execute...
    // Should this be configurable??
    cmdline += "\n";
    terminal.SendCmd(cmdline);

    // We have executed the shell - even though it might require a long time to execute this is needed
    // like 'make' and such...  we don't track that, so basically you can send more commands down to the shell
    NewLine();
    auto screen = RuntimeConfig::Instance().Screen();
    screen->Scroll(1);
}

//
// Verify if the current line can possibly be an internal command..
//
bool CommandMode::DoesCmdLineHavePrefix(std::string &cmdline) {
    if (cmdline[0] != cmdletPrefix) return false;
    if (cmdline.size() < 2) return false;

    return true;
}


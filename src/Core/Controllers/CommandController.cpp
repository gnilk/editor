//
// Created by gnilk on 15.02.23.
//

#include "CommandController.h"
#include "Core/RuntimeConfig.h"
#include "Core/StrUtil.h"
#include "Core/Config/Config.h"

using namespace gedit;

void CommandController::Begin() {
    logger = gnilk::Logger::GetLogger("CommandController");
    logger->Debug("Begin");

    NewLine();

    terminal.SetStdoutDelegate([this](std::string &output) {
        WriteLine(output);
    });

    if (!terminal.Begin()) {
        logger->Error("Unable to start terminal");
        return;
    }
}

void CommandController::NewLine() {
    if (currentLine != nullptr) {
        currentLine->SetActive(false);
    }
    std::lock_guard<std::mutex> guard(lineLock);
    currentLine = new Line();
    currentLine->SetActive(true);
    historyBuffer.push_back(currentLine);
    if (onNewLine != nullptr) {
        onNewLine();
    }
}

void CommandController::WriteLine(const std::string &str) {
    //logger->Debug("WriteLine: %s", str.c_str());
    currentLine->Append(str);
    NewLine();
}


bool CommandController::HandleKeyPress(Cursor &cursor, size_t idxLine, const KeyPress &keyPress) {
    if (DefaultEditLine(cursor, currentLine, keyPress)) {
        return true;
    }
//    if (keyPress.IsSpecialKeyPressed(Keyboard::kKeyCode_Return)) {
//        CommitLine();
//        return true;
//    }
    return false;
}

void CommandController::CommitLine() {
    std::string cmdLine(currentLine->Buffer().data());
    if (cmdLine.size() < 1) {
        return;
    }

    if (TryExecuteInternalCmd(cmdLine)) {
        return;
    }
    TryExecuteShellCmd(cmdLine);
}

bool CommandController::TryExecuteInternalCmd(std::string &cmdline) {
    auto prefix = Config::Instance()["commandmode"].GetStr("cmdlet_prefix");
    if (!strutil::startsWith(cmdline, prefix)) {
        return false;
    }
    std::string cmdLineNoPrefix = cmdline.substr(prefix.length());
    std::vector<std::string> commandList;
    // We should have a 'smarter' that keeps strings and so forth
    strutil::split(commandList, cmdLineNoPrefix.c_str(), ' ');

    //
    //
    //

    // There is more to come...
    if ((commandList[0] == "q") || (commandList[0]=="quit")) {
        exit(1);
    }

    WriteLine("internal execute: " + commandList[0]);

    return true;
}
void CommandController::TryExecuteShellCmd(std::string &cmdline) {
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
//    auto screen = RuntimeConfig::Instance().Screen();
//    screen->Scroll(1);

}





//
// Created by gnilk on 15.02.23.
//

#include "CommandController.h"
#include "Core/RuntimeConfig.h"
#include "Core/StrUtil.h"
#include "Core/Config/Config.h"

#include "Core/Editor.h"
#include "Core/API/EditorAPI.h"
#include "Core/Runloop.h"
#include "Core/Views/ListSelectionModal.h"
#include "Core/Plugins/PluginExecutor.h"

using namespace gedit;

void CommandController::Begin() {
    logger = gnilk::Logger::GetLogger("CommandController");
    logger->Debug("Begin");

    NewLine();

    terminal.SetStdoutDelegate([this](std::u32string &output) {
        WriteLine(output);
    });

    if (!terminal.Begin()) {
        logger->Error("Unable to start terminal");
        return;
    }
}

void CommandController::NewLine() {
    std::lock_guard<std::mutex> guard(lineLock);
    currentLine = std::make_shared<Line>();
    if (onNewLine != nullptr) {
        onNewLine();
    }
}

void CommandController::WriteLine(const std::u32string &str) {
    // Let's append to current line and commit to history buffer
    currentLine->Append(str);
    historyBuffer.push_back(currentLine);
    NewLine();
}

bool CommandController::HandleKeyPress(Cursor &cursor, size_t &idxLine, const KeyPress &keyPress) {
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
    // When a line is commited, it is history..
    historyBuffer.push_back(currentLine);

    std::u32string cmdLine(currentLine->Buffer());
    if (cmdLine.size() < 1) {
        NewLine();
        return;
    }

    NewLine();

    if (PluginExecutor::ParseAndExecuteWithCmdPrefix(cmdLine)) {
        return;
    }
    TryExecuteShellCmd(cmdLine);
}

void CommandController::TryExecuteShellCmd(std::u32string &cmdline) {
    // Just push this to the shell "process"...
    strutil::trim(cmdline);
    logger->Debug("Trying shell: %s", cmdline.c_str());

    // Make sure to append CRLN otherwise the shell won't execute...
    // Should this be configurable??
    cmdline += U"\n";
    terminal.SendCmd(cmdline);

    // We have executed the shell - even though it might require a long time to execute this is needed
    // like 'make' and such...  we don't track that, so basically you can send more commands down to the shell
    NewLine();
//    auto screen = RuntimeConfig::Instance().Screen();
//    screen->Scroll(1);

}



//
// This actually works...
//
void CommandController::TestShowDialog() {
    logger->Debug("Creating dialog");
    ListSelectionModal myModal;
    myModal.AddItem("Item1");
    myModal.AddItem("Item2");
    myModal.AddItem("Item3");
    myModal.AddItem("Item4");
    Runloop::ShowModal(&myModal);

}

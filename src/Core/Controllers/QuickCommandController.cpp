//
// Created by gnilk on 15.05.23.
//

#include "QuickCommandController.h"
#include "Core/ActionHelper.h"
#include "Core/Runloop.h"
#include "Core/Editor.h"
#include "Core/Config/Config.h"

using namespace gedit;

void QuickCommandController::Enter() {
    // Hook run loop here
    if (logger == nullptr) {
        logger = gnilk::Logger::GetLogger("QuickCmdCntr");
    }
    logger->Debug("Enter...");
    cmdInput = Line::Create("");
    cursor = {};
    Runloop::SetKeypressAndActionHook(this);
}

// NOTE: This should only be called by the editor!!!
void QuickCommandController::Leave() {
    // Remove run loop hook here
    logger->Debug("Leave...");
    Runloop::SetKeypressAndActionHook(nullptr);
}

bool QuickCommandController::HandleAction(const KeyPressAction &kpAction) {
    switch(kpAction.action) {
        case kAction::kActionCycleActiveBufferNext :
            ActionHelper::SwitchToNextBuffer();
            return true;
        case kAction::kActionCycleActiveBufferPrev :
            ActionHelper::SwitchToPreviousBuffer();
            return true;
        case kAction::kActionCommitLine :
            if (ParseAndExecute()) {
                DoLeaveOnSuccess();
            }
            return true;
        default:  // By default we forward the action to the active view, this allows navigation and other things
            if (!RuntimeConfig::Instance().GetRootView().HandleAction(kpAction)) {
                return false;
            }
            DoLeaveOnSuccess();
            return true;
    }
    return false;
}

void QuickCommandController::HandleKeyPress(const KeyPress &keyPress) {
    cmdInputBaseController.DefaultEditLine(cursor, cmdInput, keyPress, true);
}

bool QuickCommandController::ParseAndExecute() {
    auto cmdLineNoPrefix = cmdInput->Buffer();

    std::vector<std::string> commandList;
    // We should have a 'smarter' that keeps strings and so forth
    strutil::split(commandList, cmdLineNoPrefix.data(), ' ');

    // There is more to come...
    if (!RuntimeConfig::Instance().HasPluginCommand(commandList[0])) {
        logger->Error("Plugin '%s' not found", commandList[0].c_str());
        return false;
    }

    auto cmd = RuntimeConfig::Instance().GetPluginCommand(commandList[0]);
    auto argStart = commandList.begin()+1;
    auto argEnd = commandList.end();
    auto argList = std::vector<std::string>(argStart, argEnd);
    cmd->Execute(argList);

    return true;
}

void QuickCommandController::DoLeaveOnSuccess() {
    bool autoLeave = Config::Instance()["quickmode"].GetBool("leave_automatically", false);

    // We can (and should) always reset these...
    cmdInput = Line::Create("");
    cursor = {};

    if (autoLeave) {
        Editor::Instance().LeaveCommandMode();
        return;
    }
}

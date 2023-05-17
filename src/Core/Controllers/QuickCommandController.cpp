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
    if ((cmdInput->Buffer().length() > 4) && (cmdInput->Buffer().at(0) == '/')) {
        // we are searching, so let's update this in realtime
        std::string searchItem = std::string(cmdInput->Buffer().substr(1));
        SearchInActiveEditorModel(searchItem);
    }
}

bool QuickCommandController::ParseAndExecute() {
    auto cmdLineNoPrefix = cmdInput->Buffer();

    std::vector<std::string> commandList;
    // We should have a 'smarter' that keeps strings and so forth
    strutil::split(commandList, cmdLineNoPrefix.data(), ' ');

    // There is more to come...
    if (!RuntimeConfig::Instance().HasPluginCommand(commandList[0])) {
        logger->Error("Plugin '%s' not found", commandList[0].c_str());
        if ((commandList[0].size() > 0) && (commandList[0].at(0) == '/')) {
            // search
            std::string searchItem = commandList[0].substr(1);
            SearchInActiveEditorModel(searchItem);
        }
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

void QuickCommandController::SearchInActiveEditorModel(const std::string &searchItem) {
    auto model = Editor::Instance().GetActiveModel();
    model->ClearSearchResults();
    if (searchItem.length() < 1) {
        return;
    }
    auto numHits = model->SearchFor(searchItem);
    char tmp[32];
    snprintf(tmp,32,"Found: %d", numHits);
    RuntimeConfig::Instance().OutputConsole()->WriteLine(tmp);
}


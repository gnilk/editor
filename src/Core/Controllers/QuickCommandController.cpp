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
    if (isSearchMode) {
        HandleKeyPress(kpAction.keyPress);
        // Leave search mode in case of enter
        if (kpAction.action == kAction::kActionCommitLine) {
            logger->Debug("Leaving search");
            isSearchMode = false;
            return true;
        }
        return true;
    }

    switch(kpAction.action) {
        case kAction::kActionCycleActiveBufferNext :
            ActionHelper::SwitchToNextBuffer();
            return true;
        case kAction::kActionCycleActiveBufferPrev :
            ActionHelper::SwitchToPreviousBuffer();
            return true;
        case kAction::kActionStartSearch :
            isSearchMode = true;
            idxHit = 0;
            logger->Debug("Entering search");
            break;
        case kAction::kActionNextSearchResult :
            NextSearchResult();
            break;
        case kAction::kActionPrevSearchResult :
            PrevSearchResult();
            break;
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
    if ((cmdInput->Buffer().length() > 4) && (isSearchMode)) {
        // Use the 'substr' if we keep the 'search' character visible in the cmd-input...
        //std::string searchItem = std::string(cmdInput->Buffer().substr(1));
        std::string searchItem = std::string(cmdInput->Buffer());

        // we are searching, so let's update this in realtime
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
    model->JumpToSearchHit(idxHit);
    snprintf(tmp,32,"Found: %d", numHits);
    RuntimeConfig::Instance().OutputConsole()->WriteLine(tmp);
}

// Move these to model, this allows the model to operate over the search-results how they were
// obtained...
void QuickCommandController::NextSearchResult() {
    auto model = Editor::Instance().GetActiveModel();
    idxHit++;
    if (!model->JumpToSearchHit(idxHit) && (idxHit > 0)) {
        idxHit -=1;
    }
}
void QuickCommandController::PrevSearchResult() {
    auto model = Editor::Instance().GetActiveModel();
    if (idxHit > 0) {
        idxHit -= 1;
    }
    model->JumpToSearchHit(idxHit);
}

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
    ChangeState(State::CommandState);
    Runloop::SetKeypressAndActionHook(this);
}

// NOTE: This should only be called by the editor!!!
void QuickCommandController::Leave() {
    // Remove run loop hook here
    logger->Debug("Leave...");
    Runloop::SetKeypressAndActionHook(nullptr);
}

bool QuickCommandController::HandleAction(const KeyPressAction &kpAction) {

    if (state == State::SearchState) {
        // Leave via 'esc' - i.e. we don't save search results
        if (HandleActionInSearch(kpAction)) {
            return true;
        }
        // None of the above - pass it on, since 'QuickCommandMode' translates several
        // regular ASCII key-codes to actions, we need to do this here..
        HandleKeyPress(kpAction.keyPress);
        return true;
    }

    // We are in the command state - let's just pass it on...
    return HandleActionInCommandState(kpAction);
}
bool QuickCommandController::HandleActionInCommandState(const KeyPressAction &kpAction) {
    switch(kpAction.action) {
        case kAction::kActionCycleActiveBufferNext :
            ActionHelper::SwitchToNextBuffer();
            return true;
        case kAction::kActionCycleActiveBufferPrev :
            ActionHelper::SwitchToPreviousBuffer();
            return true;
        case kAction::kActionStartSearch :
            Editor::Instance().GetActiveModel()->ResetSearchHitIndex();
            logger->Debug("Entering search, key: %c",kpAction.keyPress.key);
            ChangeState(State::SearchState);
            cmdInputBaseController.DefaultEditLine(cursor, cmdInput, kpAction.keyPress, true);
            break;
        case kAction::kActionLastSearch :
            Editor::Instance().GetActiveModel()->ResetSearchHitIndex();
            logger->Debug("Entering search, key: %c",kpAction.keyPress.key);
            cmdInputBaseController.DefaultEditLine(cursor, cmdInput, kpAction.keyPress, true);
            if (!searchHistory.empty()) {
                auto &lastSearchItem = searchHistory.back();
                cmdInput->Append(lastSearchItem);
                cursor.position.x += lastSearchItem.length();
            }
            ChangeState(State::SearchState);
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


// Handling of translated actions while search is active..
bool QuickCommandController::HandleActionInSearch(const KeyPressAction &kpAction) {
    if (kpAction.action == kAction::kActionLeaveCommandMode) {
        logger->Debug("LeaveCommandMode, in Search - leaving");

        auto model = Editor::Instance().GetActiveModel();
        model->ResetSearchHitIndex();
        model->ClearSearchResults();
        ChangeState(State::CommandState);
        return true;
    }
    // Leave search mode in case of enter
    if (kpAction.action == kAction::kActionCommitLine) {
        std::string searchItem = std::string(cmdInput->Buffer().substr(1));
        SearchInActiveEditorModel(searchItem);

        // Add to search history...
        searchHistory.push_back(searchItem);

        logger->Debug("Leaving search");
        ChangeState(State::CommandState);
        return true;
    }
    return false;
}

void QuickCommandController::HandleKeyPress(const KeyPress &keyPress) {
    cmdInputBaseController.DefaultEditLine(cursor, cmdInput, keyPress, true);
    if (state == State::SearchState) {
        if (cmdInput->Buffer().length() == 0) {
            ChangeState(State::CommandState);
            auto model = Editor::Instance().GetActiveModel();
            model->ClearSearchResults();
        }
        if (cmdInput->Buffer().length() > 4) {
            // Use the 'substr' if we keep the 'search' character visible in the cmd-input...
            std::string searchItem = std::string(cmdInput->Buffer().substr(1));
            //std::string searchItem = std::string(cmdInput->Buffer());

            // we are searching, so let's update this in realtime
            SearchInActiveEditorModel(searchItem);
        } else {
            auto model = Editor::Instance().GetActiveModel();
            if (model->HaveSearchResults()) {
                model->ClearSearchResults();
            }
        }
    }
}

bool QuickCommandController::ParseAndExecute() {

    auto cmdline = std::string(cmdInput->Buffer());
    auto prefix = Config::Instance()["commandmode"].GetStr("cmdlet_prefix");

    std::string cmdLineNoPrefix;
    if (strutil::startsWith(cmdline, prefix)) {
        cmdLineNoPrefix = cmdline.substr(prefix.length());
    } else {
        // support 'raw' (no prefix) commands?
        // yes - I think so in the quick-mode we actually don't need it as we don't spawn stuff to the shell
        cmdLineNoPrefix = cmdInput->Buffer();
    }


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
    model->JumpToSearchHit(model->GetSearchHitIndex());
    snprintf(tmp,32,"Search: %s, Found: %zu", searchItem.c_str(), numHits);
    RuntimeConfig::Instance().OutputConsole()->WriteLine(tmp);
}

// Move these to model, this allows the model to operate over the search-results how they were
// obtained...
void QuickCommandController::NextSearchResult() {
    auto model = Editor::Instance().GetActiveModel();
    model->NextSearchResult();
}
void QuickCommandController::PrevSearchResult() {
    auto model = Editor::Instance().GetActiveModel();
    model->PrevSearchResult();
}

void QuickCommandController::ChangeState(QuickCommandController::State newState) {
    auto config = Config::Instance()["quickmode"];
    state = newState;
    switch(state) {
        case CommandState :
            prompt = config.GetStr("prompt_default", "C:");
            cursor.position.x = 0;
            cmdInput->Clear();
            break;
        case SearchState :
            prompt = config.GetStr("prompt_search", "S:");
            break;
        default:
            prompt = config.GetStr("prompt_default", "C:");
            break;
    }
}

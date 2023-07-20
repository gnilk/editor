//
// Created by gnilk on 15.05.23.
//

#include "QuickCommandController.h"
#include "Core/ActionHelper.h"
#include "Core/Runloop.h"
#include "Core/Editor.h"
#include "Core/Config/Config.h"
#include "Core/Plugins/PluginExecutor.h"

using namespace gedit;

// We are reusing some details from the command view - so need access here
static const std::string cfgSectionName = "commandview";


void QuickCommandController::Enter() {
    // Hook run loop here
    if (logger == nullptr) {
        logger = gnilk::Logger::GetLogger("QuickCmdCntr");
    }
    logger->Debug("Enter...");
    cmdInput = Line::Create("");
    cursor = {};
    ChangeState(State::QuickCmdState);
    Runloop::SetKeypressAndActionHook(this);
}

// NOTE: This should only be called by the editor!!!
void QuickCommandController::Leave() {
    // Remove run loop hook here
    logger->Debug("Leave...");
    Runloop::SetKeypressAndActionHook(nullptr);
}

bool QuickCommandController::HandleAction(const KeyPressAction &kpAction) {
    if (state == State::QuickCmdState) {
        return HandleActionInQuickCmdState(kpAction);
    }

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

    if (state == State::CmdLetState) {
        if (HandleActionInCmdLetState(kpAction)) {
            return true;
        }
        HandleKeyPress(kpAction.keyPress);
        return true;
    }
    return false;
}
bool QuickCommandController::HandleActionInQuickCmdState(const KeyPressAction &kpAction) {
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
        ChangeState(State::QuickCmdState);
        return true;
    }
    // Leave search mode in case of enter
    if (kpAction.action == kAction::kActionCommitLine) {
        std::string searchItem = std::string(cmdInput->Buffer().substr(1));
        SearchInActiveEditorModel(searchItem);

        // Add to search history...
        searchHistory.push_back(searchItem);

        logger->Debug("Leaving search");
        ChangeState(State::QuickCmdState);
        return true;
    }
    return false;
}

bool QuickCommandController::HandleActionInCmdLetState(const KeyPressAction &kpAction) {
    if (kpAction.action == kAction::kActionCommitLine) {
        logger->Debug("Should execute cmdlet!");
        auto cmdline = std::string(cmdInput->Buffer());
        if (!PluginExecutor::ParseAndExecuteWithCmdPrefix(cmdline)) {
            return false;
        }
        DoLeaveOnSuccess();
        return true;
    }
    return false;
}

void QuickCommandController::DoLeaveOnSuccess() {
    bool autoLeave = Config::Instance()["quickmode"].GetBool("leave_automatically", false);

    // We can (and should) always reset these...
    cmdInput = Line::Create("");
    cursor = {};

    if (autoLeave) {
        Editor::Instance().LeaveCommandMode();
        return;
    } else {
        ChangeState(State::QuickCmdState);
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
        case QuickCmdState :
            prompt = config.GetStr("prompt_default", "Q:");
            cursor.position.x = 0;
            cmdInput->Clear();
            break;
        case SearchState :
            prompt = config.GetStr("prompt_search", "S:");
            break;
        case CmdLetState :
            prompt = config.GetStr("prompt_cmdlet", "C:");
            break;
        default:
            prompt = config.GetStr("prompt_default", "C:");
            break;
    }
}

void QuickCommandController::HandleKeyPress(const KeyPress &keyPress) {
    cmdInputBaseController.DefaultEditLine(cursor, cmdInput, keyPress, true);

    if (state == State::QuickCmdState) {
        auto cmdline = std::string(cmdInput->Buffer());
        auto prefix = Config::Instance()[cfgSectionName].GetStr("cmdlet_prefix");
        if (strutil::startsWith(cmdline, prefix)) {
            // we have a cmdlet prefix, we should disable the Action stuff..
            logger->Debug("CmdLet Prefix detected!");
            ChangeState(State::CmdLetState);
        }
    }

    if (state == State::SearchState) {
        if (cmdInput->Buffer().length() == 0) {
            ChangeState(State::QuickCmdState);
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


    if (state == State::CmdLetState) {
        if (cmdInput->Buffer().length() == 0) {
            ChangeState(State::QuickCmdState);
        }
    }
}

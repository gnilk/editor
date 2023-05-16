//
// Created by gnilk on 15.05.23.
//

#include "QuickCommandController.h"
#include "Core/ActionHelper.h"
#include "Core/Runloop.h"
#include "Core/Editor.h"

using namespace gedit;

void QuickCommandController::Enter() {
    // Hook run loop here
    if (logger == nullptr) {
        logger = gnilk::Logger::GetLogger("QuickCmdCntr");
    }
    logger->Debug("Enter...");
    Runloop::SetKeypressAndActionHook(this);
}

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
            // TODO: Parse and execute from line
            Leave();
            break;
        default:        // Suppress warning about missing enum's in switch...
            break;
    }
    return false;
}

void QuickCommandController::HandleKeyPress(const KeyPress &keyPress) {
    logger->Debug("Handle keypress...");
    // TODO: Append to command buffer
}

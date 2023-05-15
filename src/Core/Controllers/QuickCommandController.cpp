//
// Created by gnilk on 15.05.23.
//

#include "QuickCommandController.h"
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

            return true;
        case kAction::kActionCycleActiveBufferPrev :
            return true;
    }
    return false;
}

void QuickCommandController::HandleKeyPress(const KeyPress &keyPress) {
    //return false;
    logger->Debug("Handle keypress...");
}

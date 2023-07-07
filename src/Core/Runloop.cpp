//
// Created by gnilk on 15.04.23.
//

#include "Editor.h"
#include "Runloop.h"
#include "RuntimeConfig.h"
#include "logger.h"
#include "KeypressAndActionHandler.h"

using namespace gedit;

bool Runloop::bQuit = false;
bool Runloop::isRunning = false;
KeypressAndActionHandler *Runloop::hookedActionHandler = nullptr;

void Runloop::SetKeypressAndActionHook(KeypressAndActionHandler *newHook) {
    hookedActionHandler = newHook;
}

void Runloop::DefaultLoop() {
    auto screen = RuntimeConfig::Instance().Screen();
    auto keyboardDriver = RuntimeConfig::Instance().Keyboard();
    auto &rootView = RuntimeConfig::Instance().GetRootView();
    auto logger = gnilk::Logger::GetLogger("DefaultLoop");


    KeypressAndActionHandler &kpaHandler {rootView};
    isRunning = true;

    while(!bQuit) {
        // Process any messages from other threads before we do anything else..
        bool redraw = false;

        if (rootView.ProcessMessageQueue() > 0) {
            redraw = true;
        }

        auto keyPress = keyboardDriver->GetKeyPress();
        if (keyPress.IsAnyValid()) {
            if (hookedActionHandler) {
                redraw = Runloop::DispatchToHandler(*hookedActionHandler, keyPress);
            } else {
                redraw = Runloop::DispatchToHandler(kpaHandler, keyPress);
            }
        }

        if (rootView.IsInvalid()) {
            redraw = true;
        }
        if (redraw == true) {
            //logger->Debug("Redraw was triggered...");
            screen->Clear();
            rootView.Draw();
            screen->Update();
        }
        // Yield the main-thread..
        std::this_thread::yield();
    }
}

void Runloop::ShowModal(ViewBase *modal) {
    // This is a special case of the main loop...

    auto screen = RuntimeConfig::Instance().Screen();
    auto keyboardDriver = RuntimeConfig::Instance().Keyboard();
    auto logger = gnilk::Logger::GetLogger("ShowModal");

    modal->SetActive(true);
    modal->Initialize();
    modal->InvalidateAll();
    screen->CopyToTexture();

    KeypressAndActionHandler &kpaHandler {*modal};

    isRunning = true;

    while((modal->IsActive()) && !bQuit) {
        // Process any messages from other threads before we do anything else..
        bool redraw = false;

        if (modal->ProcessMessageQueue() > 0) {
            redraw = true;
        }

        auto keyPress = keyboardDriver->GetKeyPress();
        if (keyPress.IsAnyValid()) {
            if (hookedActionHandler) {
                redraw = Runloop::DispatchToHandler(*hookedActionHandler, keyPress);
            } else {
                redraw = Runloop::DispatchToHandler(kpaHandler, keyPress);
            }
        }

        if (modal->IsInvalid()) {
            redraw = true;
        }
        if (redraw == true) {
            //logger->Debug("Redraw was triggered...");
            //screen->Clear();
            screen->ClearWithTexture();
            {
                auto &dc = modal->GetWindow()->GetContentDC();
                dc.Clear();

            }
            modal->Draw();
            screen->Update();
        }
        // Yield the main-thread..
        std::this_thread::yield();
    }
}

bool Runloop::DispatchToHandler(KeypressAndActionHandler &kpaHandler, KeyPress keyPress) {
    auto logger = gnilk::Logger::GetLogger("Dispatcher");
    logger->Debug("KeyPress Valid - passing on...");

    auto &keyMap = Editor::Instance().GetActiveKeyMap();
    auto kpAction = keyMap.ActionFromKeyPress(keyPress);

    if (kpAction.has_value()) {
        logger->Debug("Action '%s' found - sending to handler", keyMap.ActionName(kpAction->action).c_str());

        if (!kpaHandler.HandleAction(*kpAction)) {
            // Here I introduce yet another dependency in this class
            // While it could be handled through a lambda set on the run loop (perhaps nicer) I choose not
            // in the end we are writing a specific application - this the way I choose to dispatch otherwise unhandled actions...
            Editor::Instance().HandleGlobalAction(*kpAction);
        }
    } else {
        logger->Debug("No action for keypress, treating as regular input");
        kpaHandler.HandleKeyPress(keyPress);
    }
    // Well - this just controls the redraw for now..
    return true;
}


//
// simplified run loop that always redraws, this is used to test rendering pipeline
//
void Runloop::TestLoop() {
    auto screen = RuntimeConfig::Instance().Screen();
    auto keyboardDriver = RuntimeConfig::Instance().Keyboard();
    auto &rootView = RuntimeConfig::Instance().GetRootView();
    auto logger = gnilk::Logger::GetLogger("MainLoop");
    KeypressAndActionHandler &kpaHandler {rootView};
    isRunning = true;

    while(!bQuit) {

        rootView.ProcessMessageQueue();

        auto keyPress = keyboardDriver->GetKeyPress();
        if (keyPress.IsAnyValid()) {
            if (hookedActionHandler) {
                Runloop::DispatchToHandler(*hookedActionHandler, keyPress);
            } else {
                Runloop::DispatchToHandler(kpaHandler, keyPress);
            }
        }

        screen->Clear();
        rootView.Draw();
        screen->Update();
    }
}

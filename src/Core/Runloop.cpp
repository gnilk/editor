//
// Created by gnilk on 15.04.23.
//

#include "Editor.h"
#include "Runloop.h"
#include "RuntimeConfig.h"
#include "logger.h"


using namespace gedit;

bool Runloop::bQuit = false;

void Runloop::DefaultLoop() {
    auto screen = RuntimeConfig::Instance().Screen();
    auto keyboardDriver = RuntimeConfig::Instance().Keyboard();
    auto &rootView = RuntimeConfig::Instance().GetRootView();
    auto logger = gnilk::Logger::GetLogger("MainLoop");

    while(!bQuit) {
        // Process any messages from other threads before we do anything else..
        bool redraw = false;

        if (rootView.ProcessMessageQueue() > 0) {
            redraw = true;
        }

        auto keyPress = keyboardDriver->GetKeyPress();
        if (keyPress.IsAnyValid()) {

            logger->Debug("KeyPress Valid - passing on...");

            auto kpAction = KeyMapping::Instance().ActionFromKeyPress(keyPress);
            if (kpAction.has_value()) {
                logger->Debug("Action '%s' found - sending to RootView", KeyMapping::Instance().ActionName(kpAction->action).c_str());

                if (!rootView.OnAction(*kpAction)) {
                    // Here I introduce yet another dependency in this class
                    // While it could be handled through a lambda set on the run loop (perhaps nicer) I choose not
                    // in the end we are writing a specific application - this the way I choose to dispatch otherwise unhandled actions...
                    Editor::Instance().HandleGlobalAction(*kpAction);
                }
            } else {
                logger->Debug("No action for keypress, treating as regular input");
                rootView.OnKeyPress(keyPress);
            }
            redraw = true;
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

    while((modal->IsActive()) && !bQuit) {
        // Process any messages from other threads before we do anything else..
        bool redraw = false;

        if (modal->ProcessMessageQueue() > 0) {
            redraw = true;
        }

        auto keyPress = keyboardDriver->GetKeyPress();
        if (keyPress.IsAnyValid()) {

            logger->Debug("KeyPress Valid - passing on...");

            auto kpAction = KeyMapping::Instance().ActionFromKeyPress(keyPress);
            if (kpAction.has_value()) {
                logger->Debug("Action '%s' found - sending to RootView", KeyMapping::Instance().ActionName(kpAction->action).c_str());

                modal->OnAction(*kpAction);
            } else {
                logger->Debug("No action for keypress, treating as regular input");
                modal->OnKeyPress(keyPress);
            }
            redraw = true;
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
    }
}

//
// simplified run loop that always redraws, this is used to test rendering pipeline
//
void Runloop::TestLoop() {
    auto screen = RuntimeConfig::Instance().Screen();
    auto keyboardDriver = RuntimeConfig::Instance().Keyboard();
    auto &rootView = RuntimeConfig::Instance().GetRootView();
    auto logger = gnilk::Logger::GetLogger("MainLoop");

    while(!bQuit) {
        // Process any messages from other threads before we do anything else..
        bool redraw = true;

        if (rootView.ProcessMessageQueue() > 0) {
            redraw = true;
        }

        auto keyPress = keyboardDriver->GetKeyPress();
        if (keyPress.IsAnyValid()) {

            logger->Debug("KeyPress Valid - passing on...");

            auto kpAction = KeyMapping::Instance().ActionFromKeyPress(keyPress);
            if (kpAction.has_value()) {
                logger->Debug("Action '%s' found - sending to RootView", KeyMapping::Instance().ActionName(kpAction->action).c_str());

                rootView.OnAction(*kpAction);
            } else {
                logger->Debug("No action for keypress, treating as regular input");
                rootView.OnKeyPress(keyPress);
            }
            redraw = true;
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
    }
}

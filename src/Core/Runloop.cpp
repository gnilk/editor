//
// Created by gnilk on 15.04.23.
//

#include "Runloop.h"
#include "RuntimeConfig.h"
#include "logger.h"
#include "Editor.h"
#include "API/EditorAPI.h"


using namespace gedit;

bool Runloop::bQuit = false;

void Runloop::DefaultLoop() {
    auto screen = RuntimeConfig::Instance().Screen();
    auto keyboardDriver = RuntimeConfig::Instance().Keyboard();
    auto &rootView = RuntimeConfig::Instance().GetRootView();
    auto logger = gnilk::Logger::GetLogger("MainLoop");

    auto editorApi = Editor::Instance().GetAPI<EditorAPI>();
    editorApi->SetExitEditorDelegate([]()->void{
        Runloop::StopRunLoop();
    });



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

                rootView.OnAction(*kpAction);
            } else {
                logger->Debug("No action for keypress, treating as regular input");
                rootView.HandleKeyPress(keyPress);
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

    while((modal->IsActive()) && (bQuit != false)) {
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
                modal->HandleKeyPress(keyPress);
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

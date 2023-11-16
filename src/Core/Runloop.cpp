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
std::stack<KeypressAndActionHandler *> Runloop::kpaHandlers;
KeyMapping::Ref  Runloop::activeKeyMap = nullptr;
SafeQueue<std::unique_ptr<Runloop::Message> > Runloop::msgQueue = {};

void Runloop::SetKeypressAndActionHook(KeypressAndActionHandler *newHook) {
    hookedActionHandler = newHook;
}

void Runloop::DefaultLoop() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    auto keyboardDriver = RuntimeConfig::Instance().GetKeyboard();
    auto &rootView = RuntimeConfig::Instance().GetRootView();

    // Fetch the currently initialized keymapping
    activeKeyMap = Editor::Instance().GetActiveKeyMap();
    InstallKeymapChangeNotification();

    kpaHandlers.push({&rootView});

    isRunning = true;

    while(!bQuit) {
        // Process any messages from other threads before we do anything else..
        bool redraw = ProcessMessageQueue();

        if (rootView.IsInvalid()) {
            redraw = true;
        }
        if (redraw == true) {
            //auto logger = gnilk::Logger::GetLogger("DefaultLoop");
            //logger->Debug("Redraw was triggered...");
            screen->Clear();
            rootView.Draw();
            screen->Update();
        }
        // Yield the main-thread..

        // FIXME: this is causing threading problems, since no thread is now blocking us...
        std::this_thread::yield();

        // On linux - yield cause this thread/cpu to go 100% while sleep causes it to actually look normal..
        // using namespace std::chrono_literals;
        // std::this_thread::sleep_for(100ms);
    }
}

bool Runloop::ProcessMessageQueue() {
    if (!msgQueue.wait(250)) {
        return false;
    }
    while(!msgQueue.is_empty()) {
        auto msgOpt = msgQueue.pop();
        if (!msgOpt.has_value()) {
            continue;
        }
        auto &msg = *msgOpt.value();
        msg.Invoke();
    }
    return true;
}

void Runloop::ProcessKeyPress(KeyPress keyPress) {
    if (keyPress.IsAnyValid()) {
        DispatchToHandler(keyPress);
    }
}

void Runloop::ShowModal(ViewBase *modal) {
    // This is a special case of the main loop...
    auto screen = RuntimeConfig::Instance().GetScreen();

    activeKeyMap = Editor::Instance().GetActiveKeyMap();
    InstallKeymapChangeNotification();

    modal->SetActive(true);
    modal->Initialize();
    modal->InvalidateAll();
    screen->CopyToTexture();


    kpaHandlers.push(modal);

    isRunning = true;

    while((modal->IsActive()) && !bQuit) {
        // Process any messages from other threads before we do anything else..
        bool redraw = ProcessMessageQueue();

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
    kpaHandlers.pop();
}


void Runloop::InstallKeymapChangeNotification() {
    Editor::Instance().SetKeymapUpdateDelegate([](KeyMapping::Ref newKeymap){
        auto logger = gnilk::Logger::GetLogger("Runloop");
        logger->Debug("Keymap changed!!!!");
        activeKeyMap = newKeymap;
    });
}

bool Runloop::DispatchToHandler(KeyPress keyPress) {
    auto logger = gnilk::Logger::GetLogger("Dispatcher");

    KeypressAndActionHandler *kpaHandler = hookedActionHandler;
    if (kpaHandler == nullptr) {
        kpaHandler = kpaHandlers.top();
    }

    if (kpaHandler == nullptr) {
        fprintf(stderr, "[FATAL] RunLoop::DispatchToHandler, no kpaHandler (KeyPressAction) - this is fatal!!!\n");
        exit(1);
    }
    logger->Debug("KeyPress Valid - passing on...");

    auto kpAction = activeKeyMap->ActionFromKeyPress(keyPress);

    if (kpAction.has_value()) {
        logger->Debug("Action '%s' found - sending to handler", activeKeyMap->ActionName(kpAction->action).c_str());

        if (!kpaHandler->HandleAction(*kpAction)) {
            // Here I introduce yet another dependency in this class
            // While it could be handled through a lambda set on the run loop (perhaps nicer) I choose not
            // in the end we are writing a specific application - this the way I choose to dispatch otherwise unhandled actions...
            Editor::Instance().HandleGlobalAction(*kpAction);
        }
    } else {
        logger->Debug("No action for keypress, treating as regular input");
        kpaHandler->HandleKeyPress(keyPress);
    }
    // Well - this just controls the redraw for now..
    return true;
}


//
// simplified run loop that always redraws, this is used to test rendering pipeline
//
void Runloop::TestLoop() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    auto &rootView = RuntimeConfig::Instance().GetRootView();
    //auto logger = gnilk::Logger::GetLogger("MainLoop");
    kpaHandlers.push({&rootView});
    isRunning = true;

    while(!bQuit) {

        // Process any messages from other threads before we do anything else..
        ProcessMessageQueue();

        screen->Clear();
        rootView.Draw();
        screen->Update();
    }
}

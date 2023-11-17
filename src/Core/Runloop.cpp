//
// Created by gnilk on 15.04.23.
//

#include "Editor.h"
#include "Runloop.h"
#include "RuntimeConfig.h"
#include "logger.h"
#include "KeypressAndActionHandler.h"
#include "fmt/chrono.h"

using namespace gedit;

bool Runloop::bQuit = false;
bool Runloop::isRunning = false;
std::stack<KeypressAndActionHandler *> Runloop::kpaHandlers;
KeyMapping::Ref  Runloop::activeKeyMap = nullptr;
//SafeQueue<std::unique_ptr<Runloop::Message> > Runloop::msgQueue = {};

Runloop::MessageQueue Runloop::msgQueueA = {};
Runloop::MessageQueue Runloop::msgQueueB = {};

Runloop::MessageQueue *Runloop::incomingQueue = &Runloop::msgQueueA;
Runloop::MessageQueue *Runloop::processingQueue = &Runloop::msgQueueB;


void Runloop::SetKeypressAndActionHook(KeypressAndActionHandler *newHook) {
    if(newHook != nullptr) {
        kpaHandlers.push(newHook);
    } else {
        kpaHandlers.pop();
    }
}

void Runloop::SwapQueues() {
    // FIXME: must be atomic or thread-safe
    auto tmp = processingQueue;
    processingQueue = incomingQueue;
    incomingQueue = tmp;
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
    auto szNow = incomingQueue->size();
    processingQueue->clear();
    incomingQueue->clear();
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
    bool result = false;

    // On macOS we can't run the keyboard (nor screen) on a separate thread - thus we do it here...
#ifdef GEDIT_MACOS
    auto keyboardDriver = RuntimeConfig::Instance().GetKeyboard();
    auto kp = keyboardDriver->GetKeyPress();
    result = ProcessKeyPress(kp);
#else
    if (!incomingQueue->wait(250)) {
        return result;
    }
#endif
    auto logger = gnilk::Logger::GetLogger("RunLoop");

    // Swap so incoming becomes processing
    SwapQueues();

    int msgCount = 0;
    if (processingQueue->is_empty()) {
        return result;
    }
    result = true;

    logger->Debug("Processing Message Queue, elements: %zu", processingQueue->size());
    while(!processingQueue->is_empty()) {
        auto msgOpt = processingQueue->pop();
        if (!msgOpt.has_value()) {
            continue;
        }
        auto &msg = *msgOpt.value();
        msg.Invoke();
        msgCount++;
    }
    logger->Debug("Processing Complete: %d messages", msgCount);

    return result;
}

bool Runloop::ProcessKeyPress(KeyPress keyPress) {
    if (!keyPress.IsAnyValid()) {
        return false;
    }
    return DispatchToHandler(keyPress);
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

    auto kpaHandler = kpaHandlers.top();

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

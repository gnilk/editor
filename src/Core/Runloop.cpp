//
// Created by gnilk on 15.04.23.
//

#include "Editor.h"
#include "Runloop.h"

#include "DurationTimer.h"
#include "RuntimeConfig.h"
#include "Config/Config.h"
#include "logger.h"
#include "KeypressAndActionHandler.h"
#include "Core/UI/Views/RootView.h"
#include "fmt/chrono.h"

using namespace gedit;

bool Runloop::bQuit = false;
bool Runloop::isRunning = false;
std::stack<KeypressAndActionHandler *> Runloop::kpaHandlers;
KeyMapping::Ref  Runloop::activeKeyMap = nullptr;
//SafeQueue<std::unique_ptr<Runloop::Message> > Runloop::msgQueue = {};

Runloop::MessageQueue Runloop::msgQueueA = {};
Runloop::MessageQueue Runloop::msgQueueB = {};

std::atomic<Runloop::MessageQueue *> Runloop::incomingQueue = &Runloop::msgQueueA;
std::atomic<Runloop::MessageQueue *> Runloop::processingQueue = &Runloop::msgQueueB;

MouseClickTracker Runloop::mouseClickTracker = {};


void Runloop::SetKeypressAndActionHook(KeypressAndActionHandler *newHook) {
    if(newHook != nullptr) {
        kpaHandlers.push(newHook);
    } else {
        kpaHandlers.pop();
    }
}

void Runloop::SwapQueues() {
    // incomingQueue/processingQueue are std::atomic<MessageQueue *> so a concurrent
    // PostMessage on another thread always sees a fully-formed pointer, never a torn one.
    auto tmp = processingQueue.load();
    processingQueue.store(incomingQueue.load());
    incomingQueue.store(tmp);
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
    auto szNow = incomingQueue.load()->size();
    processingQueue.load()->clear();
    incomingQueue.load()->clear();
    auto logger = gnilk::Logger::GetLogger("RunLoop");
    while(!bQuit) {
        // Process any messages from other threads before we do anything else..
        bool redraw = ProcessMessageQueue();

        if (rootView.IsInvalid()) {
            redraw = true;
        }
        if (redraw == true) {
            DurationTimer durationTimer;
            logger->Debug("--- Default RL - Begin Redraw");
            durationTimer.Reset();
            screen->Clear();
            auto clearMS = durationTimer.Sample().count();
            rootView.Draw();
            auto uiRedrawMS = durationTimer.Sample().count();
            screen->Update();
            auto screenUpdateMS = durationTimer.Sample().count();
            logger->Debug("--- Default RL - End Redraw complete, Clear: %lld, UI: %lld, Screen: %lld", clearMS, uiRedrawMS, screenUpdateMS);
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

    // Drive the SDL event pump on the main thread — required on macOS (Cocoa) and consistent on Linux.
    // PollEvents blocks up to ~250 ms via SDL_WaitEventTimeout, translates keyboard events, and posts
    // them to incomingQueue.  Window and clipboard events are handled inside PollEvents directly.
    auto screen = RuntimeConfig::Instance().GetScreen();
    screen->PollEvents();

    auto logger = gnilk::Logger::GetLogger("RunLoop");

    // Swap so incoming becomes processing
    SwapQueues();

    int msgCount = 0;
    if (processingQueue.load()->is_empty()) {
        return result;
    }
    result = true;

    logger->Debug("Processing Message Queue, elements: %zu", processingQueue.load()->size());
    while(!processingQueue.load()->is_empty()) {
        auto msgOpt = processingQueue.load()->pop();
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

bool Runloop::ProcessMouseEvent(MouseEvent mouseEvent) {
    auto rootView = RuntimeConfig::Instance().GetRootViewAs<RootView>();

    // --- Mouse-routing trace instrumentation (temporary, remove once P1 is verified) -------
    // [fkling, note] keeping this as we will implement double-click and we might want it later
    // Shows which view the hit-test resolved to and the local row/col, mirroring the [DISP]
    // keyboard trace in DispatchToHandler. Re-enable while chasing routing/focus problems.
    //auto hit = rootView->HitTest(mouseEvent.x, mouseEvent.y);
    // fprintf(stderr, "[MOUSE] kind=%d row=%d col=%d button=%d wheelDelta=%d clicks=%d -> hit=%s\n",
    //         (int)mouseEvent.kind, mouseEvent.y, mouseEvent.x, mouseEvent.button, mouseEvent.wheelDelta,
    //         mouseEvent.clicks, hit != nullptr ? hit->GetClassName().c_str() : "(none)");
    // ----------------------------------------------------------------------------------------

    // Stamp the click count on presses (release/wheel/move pass through untouched). We run on the
    // runloop thread, so the tracker needs no lock. The threshold is refreshed from Config on each
    // press — clicks are rare, so this is free and picks up a user-config reload automatically.
    if (mouseEvent.kind == MouseEvent::kMouseEventKind_Press) {
        auto dblClickSpeed = Config::Instance()["mouse"].GetInt("dbl_click_speed", 250);
        mouseClickTracker.SetThreshold(std::chrono::milliseconds(dblClickSpeed));
        mouseEvent.clicks = mouseClickTracker.RegisterPress(
                std::chrono::steady_clock::now(), mouseEvent.x, mouseEvent.y, mouseEvent.button);
    }

    return rootView->DispatchMouse(mouseEvent);
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
        fprintf(stderr, "[FATAL] RunLoop::DispatchToHandler, no kpaHandler (EditorAction) - this is fatal!!!\n");
        exit(1);
    }
    logger->Debug("KeyPress Valid - passing on...");

    auto kpAction = activeKeyMap->ActionFromKeyPress(keyPress);

    // --- Keyboard-trace instrumentation (kept, commented) -----------------------------
    // Shows the final key/modifiers -> action resolution. Pair with the [KBD] trace in
    // SDLKeyboardDriver when chasing keyboard/keymap problems (layout-specific shortcuts,
    // external keyboards). Re-enable to see whether a keypress matched an action or fell
    // through to (none).
    // fprintf(stderr, "[DISP] key=0x%x('%c') mods=0x%.2x special=%d -> action=%s\n",
    //         keyPress.key, (keyPress.key >= 32 && keyPress.key < 127) ? (char)keyPress.key : '?',
    //         keyPress.modifiers, keyPress.isSpecialKey,
    //         kpAction.has_value() ? activeKeyMap->ActionName(kpAction->action).c_str() : "(none)");
    // ----------------------------------------------------------------------------------

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

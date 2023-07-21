//
// Created by gnilk on 15.04.23.
//

#ifndef EDITOR_RUNLOOP_H
#define EDITOR_RUNLOOP_H

#include "Core/KeypressAndActionHandler.h"
#include "Core/KeyMapping.h"
#include "Core/Views/ViewBase.h"

namespace gedit {
    class Runloop {
    public:
        static void DefaultLoop();
        static void ShowModal(ViewBase *modal);
        static void StopRunLoop() {
            bQuit = true;
        }
        static bool IsRunning() {
            return isRunning;
        }

        // Call with 'null' to disable
        static void SetKeypressAndActionHook(KeypressAndActionHandler *newHook);

        static void TestLoop();
    private:
        static bool DispatchToHandler(KeypressAndActionHandler &handler, KeyPress keyPress);
        static void InstallKeymapChangeNotification();
    private:
        static bool bQuit;
        static bool isRunning;
        static KeypressAndActionHandler *hookedActionHandler;
        static KeyMapping::Ref activeKeyMap;

    };
}

#endif //EDITOR_RUNLOOP_H

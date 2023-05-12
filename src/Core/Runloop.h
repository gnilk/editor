//
// Created by gnilk on 15.04.23.
//

#ifndef EDITOR_RUNLOOP_H
#define EDITOR_RUNLOOP_H

#include "Core/Views/ViewBase.h"

namespace gedit {
    class Runloop {
    public:
        static void DefaultLoop();
        static void ShowModal(ViewBase *modal);
        static void StopRunLoop() {
            bQuit = true;
        }

        static void TestLoop();

    private:
        static bool bQuit;
    };
}

#endif //EDITOR_RUNLOOP_H

//
// Created by gnilk on 15.05.23.
//

#ifndef EDITOR_KEYPRESSANDACTIONHANDLER_H
#define EDITOR_KEYPRESSANDACTIONHANDLER_H

#include "KeyPress.h"
#include "KeyMapping.h"

namespace gedit {
    class KeypressAndActionHandler {
    public:
        virtual bool HandleAction(const KeyPressAction &kpAction) {
            return false;
        }

        virtual void HandleKeyPress(const KeyPress &keyPress) {
            //return false;
        }
    };
}

#endif //EDITOR_KEYPRESSANDACTIONHANDLER_H

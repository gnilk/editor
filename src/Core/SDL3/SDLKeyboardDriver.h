//
// Created by gnilk on 29.03.23.
//

#ifndef EDITOR_SDLKEYBOARDDRIVER_H
#define EDITOR_SDLKEYBOARDDRIVER_H

#include "Core/KeyboardDriverBase.h"

namespace gedit {
    class SDLKeyboardDriver : public KeyboardDriverBase {
    public:
        bool Initialize() override;
        KeyPress GetKeyPress() override;
    };
}


#endif //EDITOR_SDLKEYBOARDDRIVER_H

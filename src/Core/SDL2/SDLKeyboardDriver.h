//
// Created by gnilk on 29.03.23.
//

#ifndef EDITOR_SDLKEYBOARDDRIVER_H
#define EDITOR_SDLKEYBOARDDRIVER_H

#include <SDL2/SDL.h>
#include "Core/KeyPress.h"
#include "Core/KeyboardDriverBase.h"

namespace gedit {
    class SDLKeyboardDriver : public KeyboardDriverBase {
    public:
        bool Initialize() override;
        KeyPress GetKeyPress() override;
    protected:
        std::optional<KeyPress> HandleKeyPressEvent(const SDL_Event &event);
        void CheckRemoveTextInputEventForKeyPress(const KeyPress &kp);
        KeyPress TranslateSDLEvent(const SDL_KeyboardEvent &kbdEvent);
        int TranslateScanCode(int scanCode);
        uint8_t TranslateModifiers(const uint16_t sdlModifiers);
    };
}


#endif //EDITOR_SDLKEYBOARDDRIVER_H

//
// Created by gnilk on 29.03.23.
//

#ifndef EDITOR_SDL3_SDLKEYBOARDDRIVER_H
#define EDITOR_SDL3_SDLKEYBOARDDRIVER_H

#include <optional>
#include <SDL3/SDL.h>
#include "Core/KeyPress.h"
#include "Core/KeyboardDriverBase.h"

namespace gedit::SDL3 {
    class SDLKeyboardDriver : public KeyboardDriverBase {
    public:
        SDLKeyboardDriver() = default;
        virtual ~SDLKeyboardDriver() = default;

        static KeyboardDriverBase::Ref Create();

        bool Initialize() override;
        void Close() override;

        // Called by SDLScreen::PollEvents() for each SDL event.
        // Returns a KeyPress for SDL_EVENT_KEY_DOWN and SDL_EVENT_TEXT_INPUT; empty otherwise.
        std::optional<KeyPress> ProcessEvent(const SDL_Event &event);

    protected:
        std::optional<KeyPress> HandleKeyPressEvent(const SDL_Event &event);
        void CheckRemoveTextInputEventForKeyPress(const KeyPress &kp);
        KeyPress TranslateSDLEvent(const SDL_KeyboardEvent &kbdEvent);
        int TranslateScanCode(int scanCode);
        uint8_t TranslateModifiers(SDL_Keymod sdlModifiers);
        void HookEditorClipBoard();
    protected:
        uint32_t sdlDummyEvent = 0;
    };
}


#endif //EDITOR_SDL3_SDLKEYBOARDDRIVER_H

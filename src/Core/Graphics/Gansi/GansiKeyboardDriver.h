//
// gedit::Gansi keyboard driver — translates gansi neutral KeyEvents into the editor's KeyPress and
// gansi modifier flags into the editor's modifier mask (shared with mouse handling for parity).
//
// This is an event-pump backend: GansiScreen::PollEvents() pulls events from the library and calls
// TranslateKeyEvent here, then posts the KeyPress. GetKeyPress() (the legacy pull model) is unused.
//
#ifndef GEDIT_GANSI_GANSIKEYBOARDDRIVER_H
#define GEDIT_GANSI_GANSIKEYBOARDDRIVER_H

#include "Core/UI/Graphics/KeyboardDriverBase.h"
#include "gansi/Event.h"

namespace gedit::Gansi {

    class GansiKeyboardDriver : public KeyboardDriverBase {
    public:
        GansiKeyboardDriver() = default;
        ~GansiKeyboardDriver() override = default;

        static KeyboardDriverBase::Ref Create();

        bool Initialize() override { return true; }
        KeyPress GetKeyPress() override { return {}; }

        // gansi KeyEvent -> editor KeyPress (printable -> key char; named key -> isSpecialKey + code).
        KeyPress TranslateKeyEvent(const gnilk::ansi::KeyEvent &ev) const;

        // gansi modifier flags -> Keyboard::kMod_* mask. Terminals can't tell left/right apart, so we
        // map to the Left* variants (the keymaps accept either).
        uint8_t TranslateModifiers(gnilk::ansi::Mod mods) const;
    };

}

#endif // GEDIT_GANSI_GANSIKEYBOARDDRIVER_H

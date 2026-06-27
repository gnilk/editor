//
// gansi capability sequences — enable/disable builders for the terminal modes the backend drives:
// SGR mouse, bracketed paste, focus reporting, the Kitty keyboard protocol (+ modifyOtherKeys
// fallback), and OSC 52 clipboard. Pure string builders, no I/O.
//
#ifndef GANSI_CAPS_H
#define GANSI_CAPS_H

#include <string>

namespace gnilk::ansi {

    class Caps {
    public:
        // SGR mouse (1006) + normal-tracking (1000) + button-event motion (1002).
        static std::string EnableMouse();
        static std::string DisableMouse();

        // Bracketed paste (2004).
        static std::string EnablePaste();
        static std::string DisablePaste();

        // Focus in/out reporting (1004).
        static std::string EnableFocus();
        static std::string DisableFocus();

        // Kitty keyboard progressive enhancement. `flags` defaults to 1 (disambiguate escape codes).
        // Push on enable (CSI > flags u), pop on disable (CSI < u).
        static std::string EnableKittyKeyboard(int flags = 1);
        static std::string DisableKittyKeyboard();

        // xterm modifyOtherKeys fallback for terminals without the Kitty protocol.
        static std::string EnableModifyOtherKeys();
        static std::string DisableModifyOtherKeys();

        // OSC 52 set-clipboard (selection 'c'). `utf8` is base64-encoded into the sequence. Works over
        // SSH where the terminal forwards OSC 52.
        static std::string SetClipboardOSC52(const std::string &utf8);

        // Base64 of an arbitrary byte string (standard alphabet, '=' padding). Exposed for testing.
        static std::string Base64(const std::string &data);
    };

}

#endif // GANSI_CAPS_H

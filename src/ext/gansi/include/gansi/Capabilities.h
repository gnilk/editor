//
// gansi terminal capability profile — the feature surface the backend gates its emitted sequences on.
//
// Why this exists: not every terminal understands every sequence we like to drive. The macOS built-in
// Terminal.app, in particular, does NOT speak the Kitty keyboard protocol, focus reporting, SGR mouse
// or OSC 52 — and emitting those leaves a stray glyph on screen (the enable sequence printed verbatim)
// or silently mis-parses input. Detect() inspects the environment (TERM / TERM_PROGRAM / COLORTERM) and
// produces a profile; Terminal::Open() then only emits what the profile says is supported, and the
// encoder degrades truecolor -> xterm-256 when 24-bit colour is not advertised.
//
#ifndef GANSI_CAPABILITIES_H
#define GANSI_CAPABILITIES_H

#include <functional>
#include <string>

namespace gnilk::ansi {

    struct Capabilities {
        // Defaults are the modern/full set (kitty / ghostty / foot / WezTerm). Detect() downgrades
        // individual flags for terminals known to lack a feature.
        bool truecolor = true;        // 24-bit SGR (38;2;r;g;b); else degrade to xterm-256 (38;5;n).
        bool sgrMouse = true;         // SGR extended mouse (1006). Our parser only speaks SGR, so this
                                      // flag gates mouse reporting entirely.
        bool focusReporting = true;   // focus in/out (1004).
        bool bracketedPaste = true;   // bracketed paste (2004).
        bool kittyKeyboard = true;    // Kitty keyboard progressive enhancement (CSI > flags u).
        bool osc52Clipboard = true;   // OSC 52 set-clipboard.

        // Environment lookup: returns the variable's value, or "" when unset. Injected so detection is
        // unit-testable without touching the real process environment.
        using EnvLookup = std::function<std::string(const std::string &)>;

        // Build a profile from an environment lookup (TERM, TERM_PROGRAM, COLORTERM).
        static Capabilities Detect(const EnvLookup &getEnv);

        // Convenience: read the real process environment via std::getenv.
        static Capabilities Detect();
    };

}

#endif // GANSI_CAPABILITIES_H

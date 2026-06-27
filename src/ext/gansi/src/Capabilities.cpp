//
// gansi capability detection from the environment.
//
// Strategy (env heuristics — cheap, no terminal round-trip):
//   * truecolor: COLORTERM is the canonical advertisement; a few terminals are known-good by identity
//     even without it. Anything else degrades to xterm-256, which renders correctly everywhere.
//   * Apple_Terminal (macOS built-in): no Kitty keyboard, no focus reporting, no SGR mouse, no OSC 52
//     — emitting those leaves a stray glyph or mis-parses input. Bracketed paste IS supported.
//   * no TERM / "dumb": strip everything advanced.
//
#include "gansi/Capabilities.h"

#include <cstdlib>

using namespace gnilk::ansi;

namespace {
    bool Contains(const std::string &hay, const std::string &needle) {
        return hay.find(needle) != std::string::npos;
    }
}

Capabilities Capabilities::Detect(const EnvLookup &getEnv) {
    Capabilities caps;   // modern defaults (all enabled)

    const std::string termProgram = getEnv("TERM_PROGRAM");
    const std::string term = getEnv("TERM");
    const std::string colorterm = getEnv("COLORTERM");

    // --- Truecolor: COLORTERM advertisement, or a known-truecolor terminal by identity. ---
    const bool colortermTruecolor = (colorterm == "truecolor" || colorterm == "24bit");
    const bool knownTruecolor =
        termProgram == "ghostty" || termProgram == "WezTerm" ||
        termProgram == "vscode"  || termProgram == "iTerm.app" ||
        Contains(term, "kitty")  || Contains(term, "ghostty") || Contains(term, "direct");
    caps.truecolor = colortermTruecolor || knownTruecolor;

    // --- Apple_Terminal: only bracketed paste from the advanced set. ---
    if (termProgram == "Apple_Terminal") {
        caps.kittyKeyboard = false;
        caps.focusReporting = false;
        caps.sgrMouse = false;
        caps.osc52Clipboard = false;
    }

    // --- No TERM / dumb terminal: nothing advanced. ---
    if (term.empty() || term == "dumb") {
        caps.truecolor = false;
        caps.sgrMouse = false;
        caps.focusReporting = false;
        caps.bracketedPaste = false;
        caps.kittyKeyboard = false;
        caps.osc52Clipboard = false;
    }

    return caps;
}

Capabilities Capabilities::Detect() {
    return Detect([](const std::string &name) -> std::string {
        const char *v = std::getenv(name.c_str());
        return v ? std::string(v) : std::string();
    });
}

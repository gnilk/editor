//
// gansi Capabilities tests — env (TERM / TERM_PROGRAM / COLORTERM) -> feature profile. Detection is
// driven through an injected lookup so these stay hermetic (no real getenv).
//
#include <testinterface.h>

#include <map>
#include <string>
#include <utility>

#include "gansi/Capabilities.h"

using namespace gnilk::ansi;

extern "C" {
DLL_EXPORT int test_capabilities(ITesting *t);
DLL_EXPORT int test_capabilities_modern(ITesting *t);
DLL_EXPORT int test_capabilities_appleterminal(ITesting *t);
DLL_EXPORT int test_capabilities_truecolor_env(ITesting *t);
DLL_EXPORT int test_capabilities_dumb(ITesting *t);
}

namespace {
    // Build an EnvLookup backed by a fixed map (unset variable -> "").
    Capabilities::EnvLookup Env(std::map<std::string, std::string> vars) {
        return [vars = std::move(vars)](const std::string &name) -> std::string {
            auto it = vars.find(name);
            return it == vars.end() ? std::string() : it->second;
        };
    }
}

DLL_EXPORT int test_capabilities(ITesting *t) {
    return kTR_Pass;
}

// A modern terminal (ghostty, COLORTERM=truecolor) keeps the full feature set.
DLL_EXPORT int test_capabilities_modern(ITesting *t) {
    auto caps = Capabilities::Detect(Env({
        {"TERM", "xterm-ghostty"},
        {"TERM_PROGRAM", "ghostty"},
        {"COLORTERM", "truecolor"},
    }));
    TR_ASSERT(t, caps.truecolor);
    TR_ASSERT(t, caps.kittyKeyboard);
    TR_ASSERT(t, caps.sgrMouse);
    TR_ASSERT(t, caps.focusReporting);
    TR_ASSERT(t, caps.bracketedPaste);
    TR_ASSERT(t, caps.osc52Clipboard);
    return kTR_Pass;
}

// macOS Terminal.app: bracketed paste only; no kitty / focus / SGR-mouse / OSC 52; 256-colour.
DLL_EXPORT int test_capabilities_appleterminal(ITesting *t) {
    auto caps = Capabilities::Detect(Env({
        {"TERM", "xterm-256color"},
        {"TERM_PROGRAM", "Apple_Terminal"},
        // Apple_Terminal does NOT set COLORTERM.
    }));
    TR_ASSERT(t, !caps.kittyKeyboard);
    TR_ASSERT(t, !caps.focusReporting);
    TR_ASSERT(t, !caps.sgrMouse);
    TR_ASSERT(t, !caps.osc52Clipboard);
    TR_ASSERT(t, !caps.truecolor);       // degrade to 256-colour
    TR_ASSERT(t, caps.bracketedPaste);   // supported
    return kTR_Pass;
}

// COLORTERM=truecolor enables truecolor even for an otherwise-unknown terminal; absence degrades.
DLL_EXPORT int test_capabilities_truecolor_env(ITesting *t) {
    auto on = Capabilities::Detect(Env({{"TERM", "xterm-256color"}, {"COLORTERM", "truecolor"}}));
    TR_ASSERT(t, on.truecolor);
    auto off = Capabilities::Detect(Env({{"TERM", "xterm-256color"}}));
    TR_ASSERT(t, !off.truecolor);   // no advertisement -> safe 256-colour
    return kTR_Pass;
}

// No TERM / dumb terminal: everything advanced is stripped.
DLL_EXPORT int test_capabilities_dumb(ITesting *t) {
    auto caps = Capabilities::Detect(Env({{"TERM", "dumb"}}));
    TR_ASSERT(t, !caps.truecolor);
    TR_ASSERT(t, !caps.kittyKeyboard);
    TR_ASSERT(t, !caps.sgrMouse);
    TR_ASSERT(t, !caps.focusReporting);
    TR_ASSERT(t, !caps.bracketedPaste);
    TR_ASSERT(t, !caps.osc52Clipboard);
    return kTR_Pass;
}

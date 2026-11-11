//
// How the editor picks its backend at launch, and what it hands its children (#25)
//
#include <testinterface.h>
#include <cstdlib>
#include "Core/Editor.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_launch(ITesting *t);
DLL_EXPORT int test_launch_explicit_backend_wins(ITesting *t);
DLL_EXPORT int test_launch_started_from_ui(ITesting *t);
DLL_EXPORT int test_launch_defaults(ITesting *t);
DLL_EXPORT int test_launch_take_started_from_ui(ITesting *t);
}

DLL_EXPORT int test_launch(ITesting *t) {
    return kTR_Pass;
}

// An explicit --backend is what was asked for - also from a process started from the UI (or one that inherited it)
DLL_EXPORT int test_launch_explicit_backend_wins(ITesting *t) {
    TR_ASSERT(t, Editor::ResolveBackend("headless", true, false, "sdl") == "headless");
    TR_ASSERT(t, Editor::ResolveBackend("ansi", true, true, "sdl") == "ansi");
    TR_ASSERT(t, Editor::ResolveBackend("SDL", false, true, "ansi") == "sdl");
    return kTR_Pass;
}

// Started from the desktop with no --backend: a window, whatever the session or the config says
DLL_EXPORT int test_launch_started_from_ui(ITesting *t) {
    TR_ASSERT(t, Editor::ResolveBackend("", true, true, "ansi") == "sdl");
    TR_ASSERT(t, Editor::ResolveBackend("", true, false, "headless") == "sdl");
    return kTR_Pass;
}

// No --backend, not from the UI: a terminal session gets the ANSI backend, otherwise the configured one
DLL_EXPORT int test_launch_defaults(ITesting *t) {
    TR_ASSERT(t, Editor::ResolveBackend("", false, true, "sdl") == "ansi");
    TR_ASSERT(t, Editor::ResolveBackend("", false, false, "sdl") == "sdl");
    TR_ASSERT(t, Editor::ResolveBackend("", false, false, "Headless") == "headless");
    return kTR_Pass;
}

// The launch marker is read once and taken out of the environment, so the editor's shell - and a trun or goatedit
// run from it - is not "started from the UI"
DLL_EXPORT int test_launch_take_started_from_ui(ITesting *t) {
    setenv("GEDIT_MACOS_STARTED_FROM_UI", "TRUE", 1);
#ifdef GEDIT_MACOS
    TR_ASSERT(t, Editor::TakeStartedFromUI());
#else
    TR_ASSERT(t, !Editor::TakeStartedFromUI());     // a macOS launch marker - ignored elsewhere, still removed
#endif
    TR_ASSERT(t, getenv("GEDIT_MACOS_STARTED_FROM_UI") == nullptr);
    TR_ASSERT(t, !Editor::TakeStartedFromUI());
    return kTR_Pass;
}

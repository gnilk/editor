//
// How the editor picks its backend at launch, and what it hands its children (#25)
//
#include <testinterface.h>
#include <cstdlib>
#include "Core/Editor.h"
#include "Core/XDGEnvironment.h"
#include "Core/StartupNotices.h"
#include "Core/Platform.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_launch(ITesting *t);
DLL_EXPORT int test_launch_explicit_backend_wins(ITesting *t);
DLL_EXPORT int test_launch_started_from_ui(ITesting *t);
DLL_EXPORT int test_launch_defaults(ITesting *t);
DLL_EXPORT int test_launch_take_started_from_ui(ITesting *t);
DLL_EXPORT int test_launch_no_display(ITesting *t);
DLL_EXPORT int test_launch_notices(ITesting *t);
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
    Platform::SetEnv("GEDIT_MACOS_STARTED_FROM_UI", "TRUE");
#ifdef GEDIT_MACOS
    TR_ASSERT(t, Editor::TakeStartedFromUI());
#else
    TR_ASSERT(t, !Editor::TakeStartedFromUI());     // a macOS launch marker - ignored elsewhere, still removed
#endif
    TR_ASSERT(t, getenv("GEDIT_MACOS_STARTED_FROM_UI") == nullptr);
    TR_ASSERT(t, !Editor::TakeStartedFromUI());
    return kTR_Pass;
}

// No display to open a window on is a terminal session - the ANSI backend, unless --backend says otherwise (#46)
DLL_EXPORT int test_launch_no_display(ITesting *t) {
#if defined(GEDIT_WINDOWS)
    // Always a desktop (no terminal backend yet) - also with none of the unix variables, as from cmd.exe
    TR_ASSERT(t, !XDGEnvironment::IsTerminalSession("tty", "", "", ""));
    TR_ASSERT(t, !XDGEnvironment::IsTerminalSession("tty", "", "", "10.0.0.2 52044 10.0.0.1 22"));
#elif defined(GEDIT_MACOS)
    // The window server is there whatever DISPLAY says; over ssh it isn't the user's
    TR_ASSERT(t, !XDGEnvironment::IsTerminalSession("macos", "", "", ""));
    TR_ASSERT(t, !XDGEnvironment::IsTerminalSession("macos", ":0", "", ""));
    TR_ASSERT(t, XDGEnvironment::IsTerminalSession("macos", "", "", "10.0.0.2 52044 10.0.0.1 22"));
    TR_ASSERT(t, XDGEnvironment::IsTerminalSession("macos", "localhost:10.0", "", "/dev/ttys004"));
#else
    // logind's ssh session - also with X forwarding
    TR_ASSERT(t, XDGEnvironment::IsTerminalSession("tty", "", "", "10.0.0.2 52044 10.0.0.1 22"));
    TR_ASSERT(t, XDGEnvironment::IsTerminalSession("tty", "localhost:10.0", "", ""));
    // A desktop session
    TR_ASSERT(t, !XDGEnvironment::IsTerminalSession("x11", ":0", "", ""));
    TR_ASSERT(t, !XDGEnvironment::IsTerminalSession("wayland", "", "wayland-0", ""));
    TR_ASSERT(t, !XDGEnvironment::IsTerminalSession("wayland", ":0", "wayland-0", ""));
    // The session type says a desktop, but there is no display (a container, ssh without pam_systemd, an inherited env)
    TR_ASSERT(t, XDGEnvironment::IsTerminalSession("x11", "", "", ""));
    TR_ASSERT(t, XDGEnvironment::IsTerminalSession("wayland", "", "", "10.0.0.2 52044 10.0.0.1 22"));
#endif
    // and the backend that follows: --backend still wins, a desktop launch still gets a window
    TR_ASSERT(t, Editor::ResolveBackend("", false, true, "sdl") == "ansi");
    TR_ASSERT(t, Editor::ResolveBackend("sdl", false, true, "sdl") == "sdl");
    return kTR_Pass;
}

namespace {
    struct RecordingConsole : public IOutputConsole {
        void WriteLine(const std::u32string &str) override {
            lines.push_back(str);
        }
        std::vector<std::u32string> lines;
    };
}

// Hitlist #50: start-up notices went to stderr only - lost when started through 'goat' or a launcher. Held until the
// terminal panel is up, then written there in order, after it straight through.
DLL_EXPORT int test_launch_notices(ITesting *t) {
    StartupNotices notices;
    notices.SetEchoToStderr(false);
    notices.Add("one");
    notices.Add("two");
    TR_ASSERT(t, notices.Held().size() == 2);
    TR_ASSERT(t, notices.Held()[0] == "goatedit: one");

    // No console (headless): still held
    notices.Attach(nullptr);
    TR_ASSERT(t, notices.Held().size() == 2);

    RecordingConsole console;
    notices.Attach(&console);
    TR_ASSERT(t, notices.Held().empty());
    TR_ASSERT(t, console.lines.size() == 2);
    TR_ASSERT(t, console.lines[0] == U"goatedit: one");
    TR_ASSERT(t, console.lines[1] == U"goatedit: two");

    notices.Add("three");
    TR_ASSERT(t, console.lines.size() == 3);
    TR_ASSERT(t, console.lines[2] == U"goatedit: three");
    TR_ASSERT(t, notices.Held().empty());
    return kTR_Pass;
}

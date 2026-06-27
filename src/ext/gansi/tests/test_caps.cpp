//
// gansi Caps tests — exact enable/disable byte sequences + base64 / OSC 52 clipboard.
//
#include <testinterface.h>

#include <string>

#include "gansi/Caps.h"

using namespace gnilk::ansi;

extern "C" {
DLL_EXPORT int test_caps(ITesting *t);
DLL_EXPORT int test_caps_mouse(ITesting *t);
DLL_EXPORT int test_caps_paste(ITesting *t);
DLL_EXPORT int test_caps_focus(ITesting *t);
DLL_EXPORT int test_caps_kitty(ITesting *t);
DLL_EXPORT int test_caps_modifyotherkeys(ITesting *t);
DLL_EXPORT int test_caps_base64(ITesting *t);
DLL_EXPORT int test_caps_osc52(ITesting *t);
}

DLL_EXPORT int test_caps(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_caps_mouse(ITesting *t) {
    TR_ASSERT(t, Caps::EnableMouse() == "\x1b[?1000h\x1b[?1002h\x1b[?1006h");
    TR_ASSERT(t, Caps::DisableMouse() == "\x1b[?1006l\x1b[?1002l\x1b[?1000l");
    return kTR_Pass;
}

DLL_EXPORT int test_caps_paste(ITesting *t) {
    TR_ASSERT(t, Caps::EnablePaste() == "\x1b[?2004h");
    TR_ASSERT(t, Caps::DisablePaste() == "\x1b[?2004l");
    return kTR_Pass;
}

DLL_EXPORT int test_caps_focus(ITesting *t) {
    TR_ASSERT(t, Caps::EnableFocus() == "\x1b[?1004h");
    TR_ASSERT(t, Caps::DisableFocus() == "\x1b[?1004l");
    return kTR_Pass;
}

DLL_EXPORT int test_caps_kitty(ITesting *t) {
    TR_ASSERT(t, Caps::EnableKittyKeyboard() == "\x1b[>1u");
    TR_ASSERT(t, Caps::EnableKittyKeyboard(5) == "\x1b[>5u");
    TR_ASSERT(t, Caps::DisableKittyKeyboard() == "\x1b[<u");
    return kTR_Pass;
}

DLL_EXPORT int test_caps_modifyotherkeys(ITesting *t) {
    TR_ASSERT(t, Caps::EnableModifyOtherKeys() == "\x1b[>4;2m");
    TR_ASSERT(t, Caps::DisableModifyOtherKeys() == "\x1b[>4;0m");
    return kTR_Pass;
}

DLL_EXPORT int test_caps_base64(ITesting *t) {
    // Known vectors incl. each padding length.
    TR_ASSERT(t, Caps::Base64("") == "");
    TR_ASSERT(t, Caps::Base64("f") == "Zg==");
    TR_ASSERT(t, Caps::Base64("fo") == "Zm8=");
    TR_ASSERT(t, Caps::Base64("foo") == "Zm9v");
    TR_ASSERT(t, Caps::Base64("hi") == "aGk=");
    TR_ASSERT(t, Caps::Base64("foobar") == "Zm9vYmFy");
    return kTR_Pass;
}

DLL_EXPORT int test_caps_osc52(ITesting *t) {
    // OSC 52 ; c ; <base64> ST   (ST == ESC \)
    TR_ASSERT(t, Caps::SetClipboardOSC52("hi") == std::string("\x1b]52;c;aGk=\x1b\\"));
    return kTR_Pass;
}

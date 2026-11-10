//
// gvt input encoders - byte-exact against xterm (ctlseqs "PC-Style Function Keys", "Mouse Tracking")
//
#include <testinterface.h>

#include "gvt/Input.h"

using namespace gnilk::vt;

extern "C" {
DLL_EXPORT int test_input(ITesting *t);
DLL_EXPORT int test_input_keys(ITesting *t);
DLL_EXPORT int test_input_modifiedkeys(ITesting *t);
DLL_EXPORT int test_input_text(ITesting *t);
DLL_EXPORT int test_input_paste(ITesting *t);
DLL_EXPORT int test_input_mouse(ITesting *t);
DLL_EXPORT int test_input_focus(ITesting *t);
}

DLL_EXPORT int test_input(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_input_keys(ITesting *t) {
    Modes modes;
    TR_ASSERT(t, EncodeKey(Key::kEnter, kModNone, modes) == "\r");
    TR_ASSERT(t, EncodeKey(Key::kTab, kModNone, modes) == "\t");
    TR_ASSERT(t, EncodeKey(Key::kBackspace, kModNone, modes) == "\x7f");
    TR_ASSERT(t, EncodeKey(Key::kEscape, kModNone, modes) == "\x1b");
    TR_ASSERT(t, EncodeKey(Key::kUp, kModNone, modes) == "\x1b[A");
    TR_ASSERT(t, EncodeKey(Key::kLeft, kModNone, modes) == "\x1b[D");
    TR_ASSERT(t, EncodeKey(Key::kHome, kModNone, modes) == "\x1b[H");
    TR_ASSERT(t, EncodeKey(Key::kEnd, kModNone, modes) == "\x1b[F");
    TR_ASSERT(t, EncodeKey(Key::kInsert, kModNone, modes) == "\x1b[2~");
    TR_ASSERT(t, EncodeKey(Key::kDelete, kModNone, modes) == "\x1b[3~");
    TR_ASSERT(t, EncodeKey(Key::kPageUp, kModNone, modes) == "\x1b[5~");
    TR_ASSERT(t, EncodeKey(Key::kPageDown, kModNone, modes) == "\x1b[6~");
    TR_ASSERT(t, EncodeKey(Key::kF1, kModNone, modes) == "\x1bOP");
    TR_ASSERT(t, EncodeKey(Key::kF4, kModNone, modes) == "\x1bOS");
    TR_ASSERT(t, EncodeKey(Key::kF5, kModNone, modes) == "\x1b[15~");
    TR_ASSERT(t, EncodeKey(Key::kF12, kModNone, modes) == "\x1b[24~");
    // DECCKM: arrows and Home/End in SS3 form
    modes.isCursorKeysApp = true;
    TR_ASSERT(t, EncodeKey(Key::kUp, kModNone, modes) == "\x1bOA");
    TR_ASSERT(t, EncodeKey(Key::kRight, kModNone, modes) == "\x1bOC");
    TR_ASSERT(t, EncodeKey(Key::kHome, kModNone, modes) == "\x1bOH");
    TR_ASSERT(t, EncodeKey(Key::kPageUp, kModNone, modes) == "\x1b[5~");
    return kTR_Pass;
}

DLL_EXPORT int test_input_modifiedkeys(ITesting *t) {
    Modes modes;
    modes.isCursorKeysApp = true;                    // modified keys are CSI regardless of DECCKM
    TR_ASSERT(t, EncodeKey(Key::kUp, kModShift, modes) == "\x1b[1;2A");
    TR_ASSERT(t, EncodeKey(Key::kLeft, kModAlt, modes) == "\x1b[1;3D");
    TR_ASSERT(t, EncodeKey(Key::kRight, kModCtrl, modes) == "\x1b[1;5C");
    TR_ASSERT(t, EncodeKey(Key::kEnd, kModCtrl | kModShift, modes) == "\x1b[1;6F");
    TR_ASSERT(t, EncodeKey(Key::kDelete, kModCtrl, modes) == "\x1b[3;5~");
    TR_ASSERT(t, EncodeKey(Key::kPageDown, kModShift, modes) == "\x1b[6;2~");
    TR_ASSERT(t, EncodeKey(Key::kF1, kModShift, modes) == "\x1b[1;2P");
    TR_ASSERT(t, EncodeKey(Key::kF5, kModCtrl, modes) == "\x1b[15;5~");
    // Shift+Tab, Alt+Enter (Claude's newline), Alt/Ctrl+Backspace, Alt+Esc
    TR_ASSERT(t, EncodeKey(Key::kTab, kModShift, modes) == "\x1b[Z");
    TR_ASSERT(t, EncodeKey(Key::kEnter, kModAlt, modes) == "\x1b\r");
    TR_ASSERT(t, EncodeKey(Key::kEnter, kModShift, modes) == "\r");
    TR_ASSERT(t, EncodeKey(Key::kBackspace, kModAlt, modes) == "\x1b\x7f");
    TR_ASSERT(t, EncodeKey(Key::kBackspace, kModCtrl, modes) == "\x08");
    TR_ASSERT(t, EncodeKey(Key::kEscape, kModAlt, modes) == "\x1b\x1b");
    return kTR_Pass;
}

DLL_EXPORT int test_input_text(ITesting *t) {
    TR_ASSERT(t, EncodeText(U'a', kModNone) == "a");
    TR_ASSERT(t, EncodeText(U'å', kModNone) == "\xC3\xA5");
    TR_ASSERT(t, EncodeText(U'中', kModNone) == "\xE4\xB8\xAD");
    TR_ASSERT(t, EncodeText(U'😀', kModNone) == "\xF0\x9F\x98\x80");
    // Ctrl
    TR_ASSERT(t, EncodeText(U'c', kModCtrl) == "\x03");
    TR_ASSERT(t, EncodeText(U'C', kModCtrl | kModShift) == "\x03");
    TR_ASSERT(t, EncodeText(U'z', kModCtrl) == "\x1a");
    TR_ASSERT(t, EncodeText(U' ', kModCtrl) == std::string(1, '\0'));
    TR_ASSERT(t, EncodeText(U'[', kModCtrl) == "\x1b");
    TR_ASSERT(t, EncodeText(U'\\', kModCtrl) == "\x1c");
    TR_ASSERT(t, EncodeText(U']', kModCtrl) == "\x1d");
    TR_ASSERT(t, EncodeText(U'/', kModCtrl) == "\x1f");
    TR_ASSERT(t, EncodeText(U'2', kModCtrl) == std::string(1, '\0'));
    TR_ASSERT(t, EncodeText(U'8', kModCtrl) == "\x7f");
    TR_ASSERT(t, EncodeText(U'å', kModCtrl) == "\xC3\xA5");     // no control form: the character
    // Alt as meta
    TR_ASSERT(t, EncodeText(U'b', kModAlt) == "\x1b" "b");
    TR_ASSERT(t, EncodeText(U'x', kModAlt | kModCtrl) == "\x1b\x18");
    return kTR_Pass;
}

DLL_EXPORT int test_input_paste(ITesting *t) {
    Modes modes;
    TR_ASSERT(t, EncodePaste("one\ntwo\r\nthree\r", modes) == "one\rtwo\rthree\r");
    modes.isBracketedPaste = true;
    TR_ASSERT(t, EncodePaste("ls\n", modes) == "\x1b[200~ls\r\x1b[201~");
    // A paste cannot close the bracket itself
    TR_ASSERT(t, EncodePaste("a\x1b[201~rm -rf\n", modes) == "\x1b[200~arm -rf\r\x1b[201~");
    TR_ASSERT(t, EncodePaste("", modes) == "\x1b[200~\x1b[201~");
    return kTR_Pass;
}

DLL_EXPORT int test_input_mouse(ITesting *t) {
    Modes modes;
    // Off: nothing
    TR_ASSERT(t, EncodeMouse(MouseButton::kLeft, MouseAction::kPress, 0, 0, kModNone, modes).empty());

    modes.mouseTracking = MouseTracking::kNormal;
    modes.mouseEncoding = MouseEncoding::kSgr;
    TR_ASSERT(t, EncodeMouse(MouseButton::kLeft, MouseAction::kPress, 4, 9, kModNone, modes) == "\x1b[<0;5;10M");
    TR_ASSERT(t, EncodeMouse(MouseButton::kLeft, MouseAction::kRelease, 4, 9, kModNone, modes) == "\x1b[<0;5;10m");
    TR_ASSERT(t, EncodeMouse(MouseButton::kRight, MouseAction::kPress, 0, 0, kModCtrl, modes) == "\x1b[<18;1;1M");
    TR_ASSERT(t, EncodeMouse(MouseButton::kWheelUp, MouseAction::kPress, 2, 3, kModNone, modes) == "\x1b[<64;3;4M");
    TR_ASSERT(t, EncodeMouse(MouseButton::kWheelDown, MouseAction::kPress, 2, 3, kModShift, modes) ==
                 "\x1b[<69;3;4M");
    TR_ASSERT(t, EncodeMouse(MouseButton::kWheelUp, MouseAction::kRelease, 2, 3, kModNone, modes).empty());
    // Motion only with 1002 (button held) / 1003 (any)
    TR_ASSERT(t, EncodeMouse(MouseButton::kLeft, MouseAction::kMotion, 1, 1, kModNone, modes).empty());
    modes.mouseTracking = MouseTracking::kButtonEvent;
    TR_ASSERT(t, EncodeMouse(MouseButton::kLeft, MouseAction::kMotion, 1, 1, kModNone, modes) == "\x1b[<32;2;2M");
    TR_ASSERT(t, EncodeMouse(MouseButton::kNone, MouseAction::kMotion, 1, 1, kModNone, modes).empty());
    modes.mouseTracking = MouseTracking::kAnyEvent;
    TR_ASSERT(t, EncodeMouse(MouseButton::kNone, MouseAction::kMotion, 1, 1, kModNone, modes) == "\x1b[<35;2;2M");

    // X10 byte encoding: 32 + code, 32 + 1-based coordinates; any release is button 3
    modes.mouseTracking = MouseTracking::kNormal;
    modes.mouseEncoding = MouseEncoding::kDefault;
    TR_ASSERT(t, EncodeMouse(MouseButton::kLeft, MouseAction::kPress, 0, 0, kModNone, modes) == "\x1b[M !!");
    TR_ASSERT(t, EncodeMouse(MouseButton::kRight, MouseAction::kRelease, 1, 2, kModNone, modes) == "\x1b[M#\"#");
    TR_ASSERT(t, EncodeMouse(MouseButton::kLeft, MouseAction::kPress, 300, 0, kModNone, modes).empty());
    // X10 tracking (?9): presses only, no modifiers
    modes.mouseTracking = MouseTracking::kX10;
    TR_ASSERT(t, EncodeMouse(MouseButton::kLeft, MouseAction::kPress, 0, 0, kModCtrl, modes) == "\x1b[M !!");
    TR_ASSERT(t, EncodeMouse(MouseButton::kLeft, MouseAction::kRelease, 0, 0, kModNone, modes).empty());
    return kTR_Pass;
}

DLL_EXPORT int test_input_focus(ITesting *t) {
    Modes modes;
    TR_ASSERT(t, EncodeFocus(true, modes).empty());
    modes.isFocusEvents = true;
    TR_ASSERT(t, EncodeFocus(true, modes) == "\x1b[I");
    TR_ASSERT(t, EncodeFocus(false, modes) == "\x1b[O");
    return kTR_Pass;
}

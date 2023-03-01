//
// Created by gnilk on 14.01.23.
//

#include <ncurses.h>
#include "Core/KeyCodes.h"
#include "Core/NCurses/NCursesKeyboardDriver.h"
#include "Core/RuntimeConfig.h"
#include "Core/KeyCodes.h"

using namespace gedit;

// FIXME: Review this..
static std::map<int, int> ncurses_translation_map_new = {
        {KEY_LEFT, kKey_Left},
        {KEY_RIGHT, kKey_Right},
        {KEY_UP, kKey_Up},
        {KEY_DOWN, kKey_Down},
        {KEY_BACKSPACE, kKey_Backspace},
        {KEY_DC, kKey_Delete},  // DC = Delete Char
        {KEY_HOME, kKey_Home},
        {KEY_END, kKey_End},
        {KEY_BTAB, kKey_Tab },
        {KEY_SRIGHT, kKey_Right},   // Note: Shift handled by kbdMonitor
        {KEY_SLEFT, kKey_Right},    // Note: Shift handled by kbdMonitor
        {KEY_PPAGE, kKey_PageUp},
        {KEY_NPAGE, kKey_PageDown},
        // The following has no formal definition in NCurses but are standard ASCII codes
        {9, kKey_Tab},
        {10, kKey_Return},
        { 27, kKey_Escape},     // ^]
        {127, kKey_Backspace},  // Certain macOS keyboards
};


NCursesKeyboardDriverNew::KeyPress NCursesKeyboardDriverNew::GetKeyPress() {
    KeyPress keyPress;
    auto rootView = RuntimeConfig::Instance().View();

    auto ch = getch();
    if (!kbdEvents.empty()) {
        while ((ch = getch()) == ERR) {
            // FIXME: timeout handling
        }
        keyPress.isHwEventValid = true;
        // Remove and assign the last to match with the getch
        while (!kbdEvents.empty()) {
            keyPress.hwEvent = kbdEvents.pop();
        }
    }
    keyPress.isKeyValid = (ch == ERR) ? false : true;
    keyPress.modifiers = ptrKeyboardMonitor->GetModifiersCurrentlyPressed();
    keyPress.key = TranslateNCurseKey(ch);
    // FIXME: I think this is macos specific...
    // In case of LeftCommand, NCurses will send another key (this is to deal with CTRL/CMD+<char>) - we don't want
    // this behaviour so we consume the other key (which is the <char> key pressed while holding CTRL/CMD)
    if ((keyPress.modifiers & Keyboard::kModifierKeys::kMod_LeftCommand) && (keyPress.isHwEventValid)) {
        ch = getch();
        keyPress.key = TranslateNCurseKey(ch);
    }
    return keyPress;

}


int NCursesKeyboardDriverNew::TranslateNCurseKey(int ch) {
    // If we have this in the translation map we translate it..
    if (ncurses_translation_map_new.find(ch) != ncurses_translation_map_new.end()) {
        return ncurses_translation_map_new[ch];
    }
    return ch;
}













///////////////////////
//
// REMOVE THIS IS THE OLD
//

// NCurses variant
bool NCursesKeyboardDriver::Initialize() {
    // TODO: Kick off monitoring here
    if (!kbdMonitor.Start()) {
        printf("ERR: Unable to start keyboard monitor for trapping of special keys (SHIFT, CTRL, CMD, etc..)\n");
        printf("Try fix this by enable 'Monitoring Input' for Terminal in macOS settings\n");
        printf("Otherwise disable this feature in the configuration file 'enable_keyboard_monitor = false'\n");
        printf("Hint: If disabled, you should consider using a different keyboard mapping than default!\n");
        return false;
    }
    kbdMonitor.SetOnKeyPressDelegate([this](Keyboard::HWKeyEvent &event){
        // We only push 'pressed'
        if (event.isPressedDown) {
            keyEventQueue.push(event);
        }
    });
    return true;
}

KeyPress NCursesKeyboardDriver::GetCh() const {
    auto ch = getch();
    return Translate(ch);
}

static std::map<int, int> ncurses_translation_map = {
        {KEY_LEFT, kKey_Left},
        {KEY_RIGHT, kKey_Right},
        {KEY_UP, kKey_Up},
        {KEY_DOWN, kKey_Down},
        {KEY_BACKSPACE, kKey_Backspace},
        {KEY_DC, kKey_Delete},  // DC = Delete Char
        {KEY_HOME, kKey_Home},
        {KEY_END, kKey_End},
        {KEY_BTAB, kKey_Tab },
        {KEY_SRIGHT, kKey_Right},   // Note: Shift handled by kbdMonitor
        {KEY_SLEFT, kKey_Right},    // Note: Shift handled by kbdMonitor
        {KEY_PPAGE, kKey_PageUp},
        {KEY_NPAGE, kKey_PageDown},
        // The following has no formal definition in NCurses but are standard ASCII codes
        {9, kKey_Tab},
        {10, kKey_Return},
        { 27, kKey_Escape},     // ^]
        {127, kKey_Backspace},  // Certain macOS keyboards
};

static KeyPress kp = {.data = {.special = kKeyCtrl_None, .code = kKey_Tab}};

// TODO: This needs to be fixed in order to use the new keyboard driver...
KeyPress NCursesKeyboardDriver::Translate(int ch) const {
    KeyPress key;

    key.data.code = kKey_NoKey_InQueue;
    key.data.special = kKeyCtrl_None;

    // Regardless - we set this...
    key.rawCode = ch;
    auto special = kbdMonitor.GetModifiersCurrentlyPressed();

    //
    // We should process the input queue first...
    //


    if (special) {
        // FIXME: process the queue here
        key.data.special = special;
    }

    if (ch == ERR) {
        return key;
    }

    // This is a special case..
    if (ch == KEY_RESIZE) {
        // bubble this up somehow...
        auto screenBase = RuntimeConfig::Instance().Screen();
        screenBase->SetExtScreenSizeNotificationFlag();
        return key;
    }

    if (debugMode) printw("raw: %d ", ch);

    if ((ch > 31) && (ch < 127)) {
        if (debugMode) printw("notrans ");
        key.data.code = ch;
    } else if (ncurses_translation_map.find(ch) != ncurses_translation_map.end()) {
        if (debugMode) {
            printw("trans: %d ", ncurses_translation_map[ch]);
        }
        key.data.code = ncurses_translation_map[ch];
    } else {
        if(debugMode) printw("not found: %d, %s ", ch, keyname(ch));
    }

    // Just set this one for now..
    //key.editorkey = (int64_t(key.data.special)<<32) | (key.data.code);

    return key;
}

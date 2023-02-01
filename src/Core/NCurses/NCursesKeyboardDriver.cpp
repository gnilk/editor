//
// Created by gnilk on 14.01.23.
//

#include <ncurses.h>
#include "Core/KeyCodes.h"
#include "Core/NCurses/NCursesKeyboardDriver.h"
#include "Core/RuntimeConfig.h"

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
    return true;
}

KeyPress NCursesKeyboardDriver::GetCh() {
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

KeyPress NCursesKeyboardDriver::Translate(int ch) {
    KeyPress key {kKey_NoKey_InQueue};

    // Regardless - we set this...
    key.rawCode = ch;

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

    key.data.special = kbdMonitor.GetSpecialCurrentlyPressed();
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

//
// Created by gnilk on 14.01.23.
//

#include <ncurses.h>
#include "Core/KeyCodes.h"
#include "Core/NCurses/NCursesKeyboardDriver.h"
#include "Core/KeyCodes.h"
#include "Core/RuntimeConfig.h"
#include "logger.h"

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


KeyPress NCursesKeyboardDriver::GetKeyPress() {
    KeyPress keyPress;
    auto currentWindow = RuntimeConfig::Instance().Window();
    WINDOW *winPtr = stdscr;
    if (currentWindow != nullptr) {
        winPtr = (WINDOW *)currentWindow->GetNativeWindow();
    }
    auto logger = gnilk::Logger::GetLogger("NCursesKeyboardDriver");

    //auto ch = wgetch(winPtr);
    auto ch = getch();

    if (!kbdEvents.empty()) {
        logger->Debug("kbdEvent not empty, ch=%d", ch);
        while ((ch = getch()) == ERR) {
            // FIXME: timeout handling
        }
        logger->Debug("  ch=%d (%s)", ch, keyname(ch));
        keyPress.isHwEventValid = true;
        // Remove and assign the last to match with the getch
        while (!kbdEvents.empty()) {
            keyPress.hwEvent = kbdEvents.pop();
        }
    } else {
        if (ch != ERR) {
            logger->Debug("kbdEvent empty, ch=%d (%s)", ch, keyname(ch));
        }
    }

    // TODO: trigger this somehow, we don't have an event loop of somekind - which makes it a bit akward..
    //       either we put this up in the translation map and leave it up to the applicaiton to handle, or something else...
    if (ch == KEY_RESIZE) {

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


int NCursesKeyboardDriver::TranslateNCurseKey(int ch) {
    // If we have this in the translation map we translate it..
    if (ncurses_translation_map_new.find(ch) != ncurses_translation_map_new.end()) {
        return ncurses_translation_map_new[ch];
    }
    return ch;
}
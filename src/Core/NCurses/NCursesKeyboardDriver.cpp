//
// Created by gnilk on 14.01.23.
//

#include <ncurses.h>
#include "Core/Config/Config.h"
#include "Core/Keyboard.h"
#include "Core/NCurses/NCursesKeyboardDriver.h"
#include "Core/KeyboardBaseMonitor.h"

#ifdef GEDIT_MACOS
#include "Core/macOS/MacOSKeyboardMonitor.h"
#endif

#include "Core/Keyboard.h"
#include "Core/RuntimeConfig.h"
#include "logger.h"
#include <memory>

using namespace gedit;

// FIXME: Review this..
static std::map<int, int> ncurses_translation_map_new = {
        {KEY_LEFT, Keyboard::kKeyCode_LeftArrow},
        {KEY_RIGHT, Keyboard::kKeyCode_RightArrow},
        {KEY_UP, Keyboard::kKeyCode_UpArrow},
        {KEY_DOWN, Keyboard::kKeyCode_DownArrow},
        {KEY_BACKSPACE, Keyboard::kKeyCode_Backspace},
        {KEY_DC, Keyboard::kKeyCode_DeleteForward},  // DC = Delete Char
        {KEY_HOME, Keyboard::kKeyCode_Home},
        {KEY_END, Keyboard::kKeyCode_End},
        {KEY_BTAB, Keyboard::kKeyCode_Tab},
        {KEY_SRIGHT, Keyboard::kKeyCode_RightArrow},   // Note: Shift handled by kbdMonitor
        {KEY_SLEFT, Keyboard::kKeyCode_LeftArrow},    // Note: Shift handled by kbdMonitor
        {KEY_PPAGE, Keyboard::kKeyCode_PageUp},
        {KEY_NPAGE, Keyboard::kKeyCode_PageDown},
        // The following has no formal definition in NCurses but are standard ASCII codes
        {9, Keyboard::kKeyCode_Tab},
        {10, Keyboard::kKeyCode_Return},
        { 27, Keyboard::kKeyCode_Escape},     // ^]
        {127, Keyboard::kKeyCode_Backspace},  // Certain macOS keyboards
};


#ifdef GEDIT_MACOS
// This depends on the OS/Backend - consider creating a platform layer or something to handle this...
static MacOSKeyboardMonitor keyboardMonitor;
#else
static KeyboardBaseMonitor keyboardMonitor;
#endif

// We don't handle multiple instances just yet...
KeyboardDriverBase::Ref NCursesKeyboardDriver::Create() {
    auto instance = std::make_shared<NCursesKeyboardDriver>();

    if (!keyboardMonitor.Start()) {
        printf("FATAL: Unable to start keyboard monitor!");
        exit(1);
    }

    instance->Begin(&keyboardMonitor);
    return instance;
}


void NCursesKeyboardDriver::Begin(KeyboardBaseMonitor *monitor) {
    ptrKeyboardMonitor = monitor;

    ptrKeyboardMonitor->SetOnKeyPressDelegate([this](Keyboard::HWKeyEvent &event) {
        kbdEvents.push(event);
    });

    timeoutGetChMSec = Config::Instance()["ncurses"].GetInt("timeoutKeyEvent", 150);
    auto logger = gnilk::Logger::GetLogger("NCursesKeyboardDriver");
    logger->Debug("NCurses Timeout: %d msec", timeoutGetChMSec);

}


KeyPress NCursesKeyboardDriver::GetKeyPress() {
    KeyPress keyPress;
//    auto currentWindow = RuntimeConfig::Instance().Window();
//    WINDOW *winPtr = stdscr;
//    if (currentWindow != nullptr) {
//        winPtr = (WINDOW *)currentWindow->GetNativeWindow();
//    }
    auto logger = gnilk::Logger::GetLogger("NCursesKeyboardDriver");

    //auto ch = wgetch(winPtr);
    auto ch = getch();

    if (!kbdEvents.is_empty()) {
        // This will flood the logger in case we are not the focus window
        // logger->Debug("kbdEvent not empty, ch=%d", ch);
        auto t1 = std::chrono::high_resolution_clock::now();

        while ((ch = getch()) == ERR) {
            auto t2 = std::chrono::high_resolution_clock::now();
            auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1);

            // This needs a bit more elaborate design,
            // Currently we bail if there are no keys coming from NCurses
            // This means we will miss keys - basically we are just extending NCurses with information from the low-level
            // However, since I simply can't figure out WHICH process/window has the keyboard input it turns out
            // my keyboard logger get's everything and thus if you use arrow keys in another window the editor will still
            // pick them up...

            if (msec.count() > timeoutGetChMSec) {
                // This will flood the logger in case we are not the focus window
                //logger->Debug("getch, timeout: %d", (int)msec.count());
                return keyPress;
            }

        }
        logger->Debug("  ch=%d (%s)", ch, keyname(ch));
        keyPress.isHwEventValid = true;
        // Remove and assign the last to match with the getch
        while (!kbdEvents.is_empty()) {
             auto event = kbdEvents.pop();
             if (!event.has_value()) {
                 break;
             }
            keyPress.hwEvent = *event;
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

    keyPress.modifiers = ptrKeyboardMonitor->GetModifiersCurrentlyPressed();

    // FIXME: I think this is macos specific...
    // In case of LeftCommand, NCurses will send another key (this is to deal with CTRL/CMD+<char>) - we don't want
    // this behaviour so we consume the other key (which is the <char> key pressed while holding CTRL/CMD)
    if ((keyPress.modifiers & Keyboard::kModifierKeys::kMod_LeftCommand) && (keyPress.isHwEventValid)) {
        ch = getch();
    }

    // We can only pass on very specific keys here...

    keyPress.isKeyValid = !(ch == ERR);
    keyPress.key = ch;
    keyPress.specialKey = TranslateNCurseKey(ch);
    keyPress.isSpecialKey = (keyPress.specialKey != -1);

    // Not sure...
    if ((keyPress.isHwEventValid) && (keyPress.hwEvent.keyCode != Keyboard::kKeyCode_None)) {
        keyPress.isSpecialKey = true;
        keyPress.specialKey = keyPress.hwEvent.keyCode;
        //keyPress.specialKey = keyPress.hwEvent.translatedScanCode;
    }

    //
    // Dump details of keypress to log
    //
    if ((keyPress.isKeyValid) || (keyPress.isHwEventValid)) {
        logger->Debug("KeyPress, isKeyValid=%s, isHwEventValid=%s, isSpecialKey=%s, modifiers=0x%x",
                      keyPress.isKeyValid ? "yes" : "no", keyPress.isHwEventValid ? "yes" : "no",
                      keyPress.isSpecialKey ? "yes" : "no", keyPress.modifiers);
        if (keyPress.isKeyValid) {
            logger->Debug("  key data, key=%d (%s)", (int)keyPress.key, keyname(keyPress.key));
        }
        if (keyPress.isSpecialKey) {
            logger->Debug("  special, key=%d", keyPress.specialKey);
        }
        if (keyPress.isHwEventValid) {
            logger->Debug("   hw data, keyCode=%d, scanCode=%d, translatedKeyCode=%d", (uint8_t)keyPress.hwEvent.keyCode,
                          keyPress.hwEvent.scanCode, (int)keyPress.hwEvent.translatedScanCode);
        }
    }


    return keyPress;

}


int NCursesKeyboardDriver::TranslateNCurseKey(int ch) {
    if (ch == ERR) {
        return -1;
    }
    // If we have this in the translation map we translate it..
    if (ncurses_translation_map_new.find(ch) != ncurses_translation_map_new.end()) {
        return ncurses_translation_map_new[ch];
    }
    auto logger = gnilk::Logger::GetLogger("NCursesKeyboardDriver");
    logger->Debug("TranslateNCurseKey, no special key found");

    return -1;
}
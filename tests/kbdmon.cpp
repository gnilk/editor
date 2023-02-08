//
// Created by gnilk on 06.02.23.
//
#include <map>
#include <unordered_map>

#include <ncurses.h>

#include "logger.h"
#include "Core/macOS/MacOSKeyboardMonitor.h"
#include "MacOSKeyMonCGEvents.h"
#include "Core/NCurses/NCursesScreen.h"
#include "Core/KeyCodes.h"
/*
typedef enum : int {
    kKeyCode_Unknown = 0,
    kKeyCode_Return = 1,
    kKeyCode_Escape,
    kKeyCode_Backspace,        // Backspace??
    kKeyCode_Tab,
    kKeyCode_Space,
    kKeyCode_F1,
    kKeyCode_F2,
    kKeyCode_F3,
    kKeyCode_F4,
    kKeyCode_F5,
    kKeyCode_F6,
    kKeyCode_F7,
    kKeyCode_F8,
    kKeyCode_F9,
    kKeyCode_F10,
    kKeyCode_F11,
    kKeyCode_F12,
    kKeyCode_PrintScreen,
    kKeyCode_ScrollLock,
    kKeyCode_Pause,
    kKeyCode_Insert,
    kKeyCode_Home,
    kKeyCode_PageUp,
    kKeyCode_DeleteForward,
    kKeyCode_End,
    kKeyCode_PageDown,
    kKeyCode_LeftArrow,
    kKeyCode_RightArraow,
    kKeyCode_DownArrow,
    kKeyCode_UpArrow,
    kKeyCode_NumLock,
    kKeyCode_MaxCodeNum,       // Don't delete this
} kKeyCode;
*/

//
// These are all non-printable editing keys, we can translate from scan-code to these
// So tactics:
// - ONLY translate what we know, this table defines the scan-code we translate (need to add KEYPAD keys to it)
// editorKeyCode {
//    uint8_t modifierMask; // this is ctrl keys (ALT, SHIFT, CMD, CTRL)
//    uint8_t keyCode;      //  This is the key code
//    uint8_t charCode;     // This is the human readable character - this should not be used unless the modifier mask has been pressed...
//    uint8_t reserved;     // future use...
// }
// modifierMask and keyCode will be 0,0 respectively if we don't know about it...
// modifierMask can be set (in cast we need to trap CTRL+<char>
//
//
static std::unordered_map<int, int> printable = {
        {0x28, Keyboard::kKeyCode_Return},
        {0x2a, Keyboard::kKeyCode_Backspace},
        {0x2b, Keyboard::kKeyCode_Tab},
        {0x2c, Keyboard::kKeyCode_Space},
        {0x35, Keyboard::kKeyCode_Escape},
        {0x3a, Keyboard::kKeyCode_F1},
        {0x3b, Keyboard::kKeyCode_F2},
        {0x3c, Keyboard::kKeyCode_F3},
        {0x3d, Keyboard::kKeyCode_F4},
        {0x3e, Keyboard::kKeyCode_F5},
        {0x3f, Keyboard::kKeyCode_F6},
        {0x40, Keyboard::kKeyCode_F7},
        {0x41, Keyboard::kKeyCode_F8},
        {0x42, Keyboard::kKeyCode_F9},
        {0x43, Keyboard::kKeyCode_F10},
        {0x44, Keyboard::kKeyCode_F11},
        {0x45, Keyboard::kKeyCode_F12},
        {0x46, Keyboard::kKeyCode_PrintScreen},
        {0x47, Keyboard::kKeyCode_ScrollLock},
        {0x48, Keyboard::kKeyCode_Pause},
        {0x49, Keyboard::kKeyCode_Insert},
        {0x4a, Keyboard::kKeyCode_Home},
        {0x4b, Keyboard::kKeyCode_PageUp},
        {0x4c, Keyboard::kKeyCode_DeleteForward},
        {0x4d, Keyboard::kKeyCode_End},
        {0x4e, Keyboard::kKeyCode_PageDown},
        {0x4f, Keyboard::kKeyCode_RightArraow},
        {0x50, Keyboard::kKeyCode_LeftArrow},
        {0x51, Keyboard::kKeyCode_DownArrow},
        {0x52, Keyboard::kKeyCode_UpArrow},
        {0x53, Keyboard::kKeyCode_NumLock},
};


// the values here are layout independent - the code actually represents a physical key - not the language representation
// <scancode>,<readable value>
static std::unordered_map<int, char> asciiTranslationMap;
static std::unordered_map<int, char> asciiShiftTranslationMap;

static int createTranslationTable() {
    int scanCode = 0x04;
    for(int i='a';i<='z';i++) {
        asciiTranslationMap[scanCode] = i;
        asciiShiftTranslationMap[scanCode] = std::toupper(i);
        scanCode++;
    }

    static std::string numbers="1234567890";
    static std::string numbersShift="!@#$%^&*()";
    for(int i=0;i<numbers.size();i++) {
        asciiTranslationMap[scanCode] = numbers[i];
        asciiShiftTranslationMap[scanCode] = numbersShift[i];
        scanCode++;
    }
    return scanCode;
}

void dumpTranslationTable(int maxRange) {
    for(int i=0;i<255;i++) {
        if (asciiTranslationMap.find(i) != asciiTranslationMap.end()) {
            printf("0x%.2x : %c (%d)", i, asciiTranslationMap[i],asciiTranslationMap[i]);
            if (asciiShiftTranslationMap.find(i) != asciiShiftTranslationMap.end()) {
                printf(" + SHIFT: %c (%d)", asciiShiftTranslationMap[i], asciiShiftTranslationMap[i]);
            }
            printf("\n");
        }
    }

}

int main(int argc, char **argv) {
    MacOSKeyboardMonitor kbdMonitor;
    //MacOSKeyMonCGEvent kbdMonitor;
    auto maxScan = createTranslationTable();
    dumpTranslationTable(maxScan);

    gnilk::Logger::Initialize();
    auto logger = gnilk::Logger::GetLogger("main");

    logger->Debug("Initialize keyboard driver");
    if (!kbdMonitor.Start()) {
        logger->Error("Unable to initialize keyboard driver - check your permissions!");
        return -1;
    }

    auto flog = fopen("kbdmon.log","w+");
    if (!flog) {
        exit(1);
    }

    kbdMonitor.SetEventNotificationsForModifiers(false);
    kbdMonitor.SetEventNotificationsForKeyUpDown(true, false);
    kbdMonitor.SetDebug(false);

    kbdMonitor.SetOnKeyPressDelegate([&kbdMonitor, &flog](Keyboard::HWKeyEvent &event) {
        fprintf(flog, "scanCode=0x%x translated=0x%x (%c)  [modifierMask=0x%.2x keyCode=0x%.2x  (pressed: %s)]\n",event.scanCode, event.translatedScanCode,event.translatedScanCode,
                event.modifiers, event.keyCode, event.isPressedDown?"down":"up");
        fflush(flog);
    });

    NCursesScreen screen;
    screen.Open();


    int y = 0;
    while(true) {

//        auto ch = getchar();
        auto ch = getch();
        if (ch == ERR) {
            continue;
        }
        attrset(A_NORMAL);
        attron(COLOR_PAIR(1));
        auto special = kbdMonitor.GetModifiersCurrentlyPressed();

        mvprintw(y,0,"%d - 0x%x - %s [driver, modifiers: 0x%.2x]", ch, ch, keyname(ch), special);
        y++;



        refresh();
    }
    fclose(flog);
}

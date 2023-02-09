//
// Created by gnilk on 06.02.23.
//
#include <map>
#include <unordered_map>
#include <thread>

#include <ncurses.h>

#include "logger.h"
#include "Core/macOS/MacOSKeyboardMonitor.h"
#include "MacOSKeyMonCGEvents.h"
#include "Core/NCurses/NCursesScreen.h"
#include "Core/KeyCodes.h"
#include "Core/SafeQueue.h"

static MacOSKeyboardMonitor kbdMonitor;


static void runmonitoring() {
    auto flog = fopen("kbdmon.log","w+");
    if (!flog) {
        exit(1);
    }

    SafeQueue<Keyboard::HWKeyEvent> kbdEvents;
    kbdMonitor.SetOnKeyPressDelegate([&kbdEvents, &flog](Keyboard::HWKeyEvent &event) {
        fprintf(flog, "scanCode=0x%x translated=0x%x (%c)  [modifierMask=0x%.2x keyCode=0x%.2x  (pressed: %s)]\n",event.scanCode, event.translatedScanCode,event.translatedScanCode,
                event.modifiers, event.keyCode, event.isPressedDown?"down":"up");
        fflush(flog);
        kbdEvents.push(event);
    });

    // Initialize NCurses - I could have used the NCurses wrapper but I really wanted to toggle things
    use_extended_names(TRUE);
    initscr();
    if (has_colors()) {
        // Just test it a bit...
        start_color();
        init_color(COLOR_GREEN, 200,1000,200);
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_BLACK, COLOR_GREEN);
        attron(COLOR_PAIR(1));
    } else {
        printf("No colors, going with defaults...\n");
    }
    timeout(1); // Make 'getch' non-blocking..

    clear();
    raw();
    keypad(stdscr, TRUE);
    scrollok(stdscr, TRUE);
    noecho();
    cbreak();
    nonl();

    attrset(A_NORMAL);
    attron(COLOR_PAIR(1));
    bool nextLine = false;

    int y = 0;
    while(true) {

        auto ch = getch();

        if (ch != ERR) {
            auto special = kbdMonitor.GetModifiersCurrentlyPressed();
            mvprintw(y,0,"%d - 0x%x - %s [driver, modifiers: 0x%.2x]", ch, ch, keyname(ch), special);
            // If we are empty, see if something enters the queue...
            nextLine = true;
        } else {
            if (!kbdEvents.empty()) {
                mvprintw(y,0,"<NC ERR>");
            }
        }

        while (!kbdEvents.empty()) {
            auto event = kbdEvents.pop();
            printw(" - scanCode=0x%x, translation=0x%x (%c)", event.scanCode, event.translatedScanCode, event.translatedScanCode);
            nextLine = true;
4        }
        if (nextLine) {
            y++;
        }
        nextLine = false;
        refresh();
    }
    fclose(flog);


}

class KeyboardHandler {
public:
    struct KeyPress {
        bool isKeyValid;
        bool isHwEventValid;
        Keyboard::HWKeyEvent hwEvent;
        uint8_t modifiers;
        int key;
    };
public:
    KeyboardHandler() = default;
    void Begin(MacOSKeyboardMonitor *monitor) {
        ptrKeyboardMonitor = monitor;

        kbdMonitor.SetOnKeyPressDelegate([this](Keyboard::HWKeyEvent &event) {
            kbdEvents.push(event);
        });
    }
    KeyPress GetKeyPress() {
        KeyPress keyPress;

        keyPress.isHwEventValid = false;

        auto ch = getch();
        if (!kbdEvents.empty()) {
            while((ch = getch()) == ERR) {
                // FIXME: timeout handling
            }
            keyPress.hwEvent = kbdEvents.pop();
            keyPress.isHwEventValid = true;

        }
        keyPress.isKeyValid = (ch==ERR)?false:true;
        keyPress.modifiers = kbdMonitor.GetModifiersCurrentlyPressed();
        keyPress.key = ch;
        return keyPress;
    }
private:
    MacOSKeyboardMonitor *ptrKeyboardMonitor;
    SafeQueue<Keyboard::HWKeyEvent> kbdEvents;

};

static void runkeyboardhandler() {
    KeyboardHandler kbdHandler;
    kbdHandler.Begin(&kbdMonitor);

    use_extended_names(TRUE);
    initscr();
    if (has_colors()) {
        // Just test it a bit...
        start_color();
        init_color(COLOR_GREEN, 200,1000,200);
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_BLACK, COLOR_GREEN);
        attron(COLOR_PAIR(1));
    } else {
        printf("No colors, going with defaults...\n");
    }
    timeout(1); // Make 'getch' non-blocking..

    clear();
    raw();
    keypad(stdscr, TRUE);
    scrollok(stdscr, TRUE);
    noecho();
    cbreak();
    nonl();

    attrset(A_NORMAL);
    attron(COLOR_PAIR(1));
    bool nextLine = false;

    int y = 0;
    int row, col;
    getmaxyx(stdscr,row,col);

    while(true) {
        auto keyPress = kbdHandler.GetKeyPress();
        if (keyPress.isKeyValid) {
            mvprintw(y,0,"%d - 0x%x - %s [driver, modifiers: 0x%.2x]", keyPress.key, keyPress.key, keyname(keyPress.key), keyPress.modifiers);
            printw("- HW:%s ", keyPress.isHwEventValid?"y":"n");
            if (keyPress.isHwEventValid) {
                printw(" - scanCode=0x%x, translation=0x%x (%c)", keyPress.hwEvent.scanCode, keyPress.hwEvent.translatedScanCode, keyPress.hwEvent.translatedScanCode);
            }
            y++;
            if (y > (row -1)) {
                y = y-1;
                scroll(stdscr);
                refresh();
            }
        }
    }
}


int main(int argc, char **argv) {


    gnilk::Logger::Initialize();
    auto logger = gnilk::Logger::GetLogger("main");

    logger->Debug("Initialize keyboard driver");
    if (!kbdMonitor.Start()) {
        logger->Error("Unable to initialize keyboard driver - check your permissions!");
        return -1;
    }

    kbdMonitor.SetEventNotificationsForModifiers(false);
    kbdMonitor.SetEventNotificationsForKeyUpDown(true, false);
    kbdMonitor.SetDebug(false);

    runkeyboardhandler();

}

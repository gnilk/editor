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
#include "Core/Keyboard.h"
#include "Core/SafeQueue.h"
#include "Core/NCurses/NCursesKeyboardDriver.h"

#include <ApplicationServices/ApplicationServices.h>

static MacOSKeyboardMonitor kbdMonitor;

static void runOnlyMonitor() {
    kbdMonitor.SetOnKeyPressDelegate([](Keyboard::HWKeyEvent &event) {
        printf("\nscanCode=0x%x translated=0x%x (%c)  [modifierMask=0x%.2x keyCode=0x%.2x  (pressed: %s)]\n",event.scanCode, event.translatedScanCode,event.translatedScanCode,
                event.modifiers, event.keyCode, event.isPressedDown?"down":"up");
    });
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

static gedit::NCursesKeyboardDriverNew keyboardDriver;

static void runMonitorWithNCurses() {
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

        // 1) We need to process NCurses input first...
        //    reason: The keyboard monitor driver will capture all keystrokes (like a keylogger)
        //    and thus we don't know if a keystroke belongs to our application or not..
        // Therefore, first take this one and then assign the last incoming from the keyboard monitor driver
        // as the special key - this can ofcourse be a bit "flaky" but it's the best I know..
        // We probably need to tune this a bit, as we want to handle special modifier combinations that
        // NCurses currently won't give us...
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

        // Now empty the event queue...
        while (!kbdEvents.empty()) {
            auto event = kbdEvents.pop();
            printw(" - scanCode=0x%x, translation=0x%x (%c)", event.scanCode, event.translatedScanCode, event.translatedScanCode);
            nextLine = true;
        }
        if (nextLine) {
            y++;
        }
        nextLine = false;
        refresh();
    }
    fclose(flog);


}


static void runkeyboardhandler() {

    keyboardDriver.Begin(&kbdMonitor);

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
        auto keyPress = keyboardDriver.GetKeyPress();
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
    //runMonitorWithNCurses();
    //runOnlyMonitor();

}

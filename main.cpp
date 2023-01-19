/*
 * Very unfancy editor
 * Some core features I want:
 * - an editor with different backends for rendering (ncurses, imgui, etc..)
 * - cmd-mode (like old Amiga AsmOne had)
 *
 * TODO:
 *  - Scrolling past screen boundaries (i.e. navigate outside visible area)
 *  - Lines extending screen (incl. wrapping)
 *  - Start cmd-let parsing...
 *  #key_sup				kUP	str	!5	KEY_SUP		+	-----	shifted up-arrow key
 */
#include <iostream>
#include <ncurses.h>
#include <string_view>
#include <vector>


#include "Core/macOS/MacOSKeyboardMonitor.h"

#include "Core/NCurses/NCursesScreen.h"
#include "Core/NCurses/NCursesKeyboardDriver.h"

#include "Core/Line.h"
#include "Core/ModeBase.h"
#include "Core/CommandMode.h"
#include "Core/EditorMode.h"
#include "Core/ScreenBase.h"
#include "Core/EditorConfig.h"
#include "Core/StrUtil.h"
#include "Core/Cursor.h"
#include "Core/KeyCodes.h"
#include "Core/KeyboardDriverBase.h"

#include "Core/RuntimeConfig.h"

#include <map>

////
// Test the keyboard handling
////

static bool LoadToBuffer(Buffer &outBuffer, const char *filename) {
    FILE *f = fopen(filename,"r");
    if (f == nullptr) {
        printf("Unable to open file\n");
        return false;
    }
    char tmp[MAX_LINE_LENGTH];
    while(fgets(tmp, MAX_LINE_LENGTH, f)) {
        outBuffer.push_back(new Line(tmp));
    }

    fclose(f);
    return true;
}


typedef enum {
    kMainState_Editor = 1,
    kMainState_Command = 2,
} kMainState;

static void testKeyboard() {
    NCursesKeyboardDriver keyBoard;

    if (!keyBoard.Initialize()) {
        exit(1);
    }

    keyBoard.SetDebugMode(true);

    initscr();
    keypad(stdscr, TRUE);
    noecho();
    cbreak();

    printw("%s\n",keyname(330));

    while(true) {
        auto keyPress = keyBoard.GetCh();
        if (!keyPress.IsValid()) {
            continue;
        }
        printw("code: %d, special: %d, raw: %d\n", keyPress.data.code,keyPress.data.special, (int)keyPress.rawCode);

//        if (KeyboardDriverBase::IsHumanReadable(key)) {
//            addch(key.data.code);
//        } else {
//            if (key.data.special != 0) {
//                printw("special: %x, %x\n", key.data.special, key.data.code);
//            }
//        }
        //printf("%d",key.data.code);
    }
}

int main(int argc, const char **argv) {
//    testKeyboard();
//    exit(1);

    CommandMode::TestExecuteShellCmd();
    exit(1);

    bool bQuit = false;
    NCursesScreen screen;
    NCursesKeyboardDriver keyBoard;

    EditorMode editorMode;
    CommandMode commandMode;

    ModeBase *currentMode = &editorMode;

    commandMode.SetOnExitApp([&bQuit]() { bQuit = true; });
    editorMode.SetOnExitApp([&bQuit]() { bQuit = true; });

    commandMode.SetOnExitMode([&screen, &currentMode, &editorMode]() {
        currentMode->OnSwitchMode(false);
        currentMode = &editorMode;
        screen.Clear();
        currentMode->OnSwitchMode(true);
    });

    editorMode.SetOnExitMode([&screen, &currentMode, &commandMode]() {
        currentMode->OnSwitchMode(false);
        currentMode = &commandMode;
        currentMode->OnSwitchMode(true);
    });

    if (argc > 1) {
        Buffer buffer;
        printf("Loading: %s\n", argv[1]);
        if (!LoadToBuffer(buffer, argv[1])) {
            printf("Unable to load: %s\n", argv[1]);
            exit(1);
        }
        printf("Ok, file '%s' loaded\n", argv[1]);
        printf("Lines: %d\n", (int)buffer.size());
        editorMode.SetBuffer(buffer);
    }

    if (!keyBoard.Initialize()) {
        return -1;
    }

    // Setup the runtime enviornment
    RuntimeConfig::Instance().SetScreen(screen);
    RuntimeConfig::Instance().SetKeyboard(keyBoard);

    screen.Open();
    screen.Clear();

    while(!bQuit) {
        currentMode->DrawLines();
        screen.Update();
        currentMode->Update();
    }
    screen.Close();
    return 0;
}

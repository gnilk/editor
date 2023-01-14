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

#include "Core/NCurses/NCursesScreen.h"

#include "Core/Line.h"
#include "Core/ModeBase.h"
#include "Core/CommandMode.h"
#include "Core/EditorMode.h"
#include "Core/ScreenBase.h"
#include "Core/EditorConfig.h"
#include "Core/StrUtil.h"
#include "Core/Cursor.h"
#include "Core/KeyCodes.h"

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

int main(int argc, const char **argv) {
    bool bQuit = false;
    NCursesScreen screen;
    kMainState state;


    EditorMode editorMode;
    CommandMode commandMode;

    ModeBase *currentMode = &editorMode;

    commandMode.SetOnExitApp([&bQuit]() { bQuit = true; });
    editorMode.SetOnExitApp([&bQuit]() { bQuit = true; });

    commandMode.SetOnExitMode([&screen, &currentMode, &editorMode]() {
        currentMode = &editorMode;
        screen.Clear();
    });

    editorMode.SetOnExitMode([&screen, &currentMode, &commandMode]() {
        currentMode = &commandMode;
        screen.Clear();
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

    screen.Open();
    screen.Clear();

    while(!bQuit) {
        currentMode->DrawLines(screen);
        screen.Update();
        currentMode->Update(screen);
    }
    screen.Close();
    return 0;
}

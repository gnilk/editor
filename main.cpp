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
#include "Core/Line.h"
#include "Core/ModeBase.h"
#include "Core/CommandMode.h"
#include "Core/EditorMode.h"
#include "Core/ScreenBase.h"
#include "Core/EditorConfig.h"
#include "Core/StrUtil.h"
#include "Core/Cursor.h"
#include "Core/KeyCodes.h"

#include <map>

////
// Test the keyboard handling
////

class KeyBoard {
public:
    virtual bool Initialize() { return false; };
    virtual KeyPress GetCh();
    static bool IsValid(KeyPress &key);
    static bool IsShift(KeyPress &key);
    static bool IsHumanReadable(KeyPress &key);
protected:

};

class NCursesKeyBoard : public KeyBoard {
public:
    bool Initialize() override;
    KeyPress GetCh() override;
protected:
    KeyPress Translate(int ch);
private:
    MacOSKeyboardMonitor kbdMonitor;
};


KeyPress KeyBoard::GetCh() {
    return {};
}

bool KeyBoard::IsValid(KeyPress &key) {
    return (key.editorkey != kKey_NoKey_InQueue);
}

bool KeyBoard::IsShift(KeyPress &key) {
    return ((key.data.special & kKeyCtrl_LeftShift) | (key.data.special & kKeyCtrl_RightShift));
}

bool KeyBoard::IsHumanReadable(KeyPress &key) {
    // Human readables..  NOTE: we don't support unicode..  I'm old-skool...
    if ((key.data.code > 31) && (key.data.code < 128)) {
        return true;
    }
    return false;
}



// NCurses variant
bool NCursesKeyBoard::Initialize() {
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

KeyPress NCursesKeyBoard::GetCh() {
    auto ch = getch();
    return Translate(ch);
}
static std::map<int, int> ncurses_translation_map = {
    {KEY_LEFT, kKey_Left},
    { KEY_RIGHT, kKey_Right},
    { KEY_UP, kKey_Up},
    {KEY_DOWN, kKey_Down},
    {KEY_BACKSPACE, kKey_Backspace},
    {KEY_DC, kKey_Delete},  // DC = Delete Char
    {KEY_HOME, kKey_Home},
    {KEY_END, kKey_End},
    {KEY_BTAB, kKey_Tab },
    // The following has no formal definition in NCurses but are standard ASCII codes
    {9, kKey_Tab},
    {10, kKey_Return},
    { 27, kKey_Escape},     // ^]
};

static KeyPress kp = {.data = {.special = kKeyCtrl_None, .code = kKey_Tab}};

KeyPress NCursesKeyBoard::Translate(int ch) {
    KeyPress key {kKey_NoKey_InQueue};

    // Regardless - we set this...
    key.rawCode = ch;

    if (ch == ERR) {
        return key;
    }
    // Assume...
    key.data.special = 0;

    if ((ch > 31) && (ch < 128)) {
        key.data.code = ch;
        return key;
    }

    if (ncurses_translation_map.find(ch) != ncurses_translation_map.end()) {
        key.data.code = ncurses_translation_map[ch];
        return key;
    }
    // Now translate whatever if missing, if any..
    return key;
}

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
    NCursesKeyBoard keyBoard;

    if (!keyBoard.Initialize()) {
        exit(1);
    }

    initscr();
    keypad(stdscr, TRUE);
    noecho();
    cbreak();

    printw("%s\n",keyname(353));

    while(true) {
        auto key = keyBoard.GetCh();
        if (!KeyBoard::IsValid(key)) {
            continue;
        }
        if (KeyBoard::IsHumanReadable(key)) {
            addch(key.data.code);
        } else {
            printw("code: %d\n", key.data.code);
        }
        //printf("%d",key.data.code);
    }
}

int main(int argc, const char **argv) {

    testKeyboard();
    exit(1);
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

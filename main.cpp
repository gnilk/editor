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
#include "Core/Config/Config.h"

#include "Core/Language/CPP/CPPLanguage.h"
#include "Core/Language/LangToken.h"


#include "Core/RuntimeConfig.h"
#include "Core/Buffer.h"

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
        outBuffer.Lines().push_back(new Line(tmp));
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

static void testConfig() {

    auto shell = Config::Instance()["terminal"].GetStr("shell", "<noshell>");
    auto shellInitStr = Config::Instance()["terminal"].GetStr("init", "-ils");
    printf("shell: %s\n", shell.c_str());
    printf("shellInit: %s\n", shellInitStr.c_str());

    auto terminal = Config::Instance()["terminal"];
    //auto shell = terminal.GetStr("shell","<noshell>");

    auto nothing = terminal.GetSequenceOfStr("dummy");
    if (nothing.empty()) {
        printf("dummy is empty\n");
    }

    auto bootstrap = terminal.GetSequenceOfStr("bootstrap");
    for(auto &s : bootstrap) {
        printf("- %s\n",s.c_str());
    }

    auto languages = Config::Instance()["languages"];
    auto langDefault = languages["default"];
    auto indent = langDefault.GetInt("indent");
    auto insertSpaces = langDefault.GetBool("insert_spaces");
    printf("Indent: %d\n", indent);
    printf("Insert Spaces: %s\n", insertSpaces?"yes":"no");

}
static void testBufferLoading(const char *filename) {
    Buffer *buffer = new Buffer();
    printf("Loading: %s\n", filename);
    if (!LoadToBuffer(*buffer, filename)) {
        printf("Unable to load: %s\n", filename);
        exit(1);
    }
    buffer->SetLanguage(Config::Instance().GetLanguageForFilename(filename));

    printf("Ok, file '%s' loaded\n", filename);
    printf("Lines: %d\n", (int)buffer->Lines().size());
//    editorMode.SetBuffer(buffer);

}

int main(int argc, const char **argv) {
    auto configOk = Config::Instance().LoadConfig("config.yml");
    if (!configOk) {
        printf("[ERR] Unable to load default configuration from 'config.yml' - defaults will be used\n");
        exit(1);
    }

    CPPLanguage cppLanguage;
    cppLanguage.Initialize();
    Config::Instance().RegisterLanguage(".cpp", &cppLanguage);

    bool bQuit = false;
    NCursesScreen screen;
    NCursesKeyboardDriver keyBoard;

    EditorMode editorMode;
    CommandMode commandMode;


    // Doesn't work with NCurses - probably messing up the TTY
    // We should try using 'forkpty'
    // Must be done early..
    if (!commandMode.Begin()) {
        return -1;
    }


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

    // FIXME: Call to 'BufferManager->CreateEmptyBuffer()'
    if (argc > 1) {
        Buffer *buffer = new Buffer();
        printf("Loading: %s\n", argv[1]);
        if (!LoadToBuffer(*buffer, argv[1])) {
            printf("Unable to load: %s\n", argv[1]);
            exit(1);
        }
        buffer->SetLanguage(Config::Instance().GetLanguageForFilename(argv[1]));

        printf("Ok, file '%s' loaded\n", argv[1]);
        printf("Lines: %d\n", (int)buffer->Lines().size());
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

    // NOTE: This must be done after the screen has been opened as the color handling might require the underlying graphics
    //       context to be initialized...
    auto &colorConfig = Config::Instance().ColorConfiguration();
    for(int i=0;gnilk::IsLanguageTokenClass(i);i++) {
        auto langClass = gnilk::LanguageTokenClassToString(static_cast<gnilk::kLanguageTokenClass>(i));
        if (!colorConfig.HasColor(langClass)) {
            printf("\nErr, missing color configuration for: %s\n", langClass.c_str());
            return -1;
        }
        screen.RegisterColor(i, colorConfig.GetColor(langClass), colorConfig.GetColor("background"));
    }


    while(!bQuit) {
        currentMode->DrawLines();
        screen.Update();
        currentMode->Update();
    }
    screen.Close();
    return 0;
}

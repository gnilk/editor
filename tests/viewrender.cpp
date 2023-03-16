//
// Created by gnilk on 13.02.23.
//
/*
 * TO-DO List
 * 1)
 * - Move EditorView 'EditViewSharedData' to TextBuffer
 * - Add ability to get 'ActiveBuffer' through BufferManager or RuntimeConfig (regardless)
 * - In BufferManager - make it possible to iterate through all buffers currently open..
 * - headerView -> Specialize SingleLineView to 'HeaderView' make the draw function
 * - new single view for status line / splitter -> make this a specific "HSplitView"
 *
 * 2)
 * - CommandView, Store/Restore splitter when view goes inactive/active
 *   Note: this can be tested before adjusting on new line
 * - CommandView should adjust height of splitter on new lines..
 *   note: once done, remove f1/f2 adjustment keys...
 *
 *
 *
 *
 * + BaseController, handle key press (take from old ModeBase/EditorMode)
 * - Consolidate NCursesKeyBoard kKeyCode_xxxx with Keyboard::kKeyCode - currently there is a mismatch..
 * + Figure out how to handle 'HasContentChanged' notfications to force redraws..
 *   a) be in the redraw loop and just do it (let the views take care of it)
 *   b) Somehow let a controller or view set a flag that a redraw is needed..
 * - CommandController, NOTE: THIS IS QUITE THE TASK
 *   a) Make it on par with the old CommandMode
 *   b) Break-out and start implement CmdLet handling
 *   c) Define the proper API for talking to the editor through the cmd-let's
 * ! Create a specific HSplitView which can support a 'split' window like feature and on-request resize
 *   both views (upper/lower) in tandem..
 * ! HSplitView - ability to a view to request 'Increased Size' by X..
 * - Create a 'StatusBar' view (Single line, no border)
 * ! HStackView, which simply 'stacks' and computes sizes accordingly when updated
 * - Import the language/color features in to this project
 * - Promote this project to the new 'main' project...
 * - BufferManager should store 'fullPathName' and 'name'
 * - Unsaved file should have '*' marking in the top..
 * ! Consider relationship between viexw/context/window - right now there is too much flexibility
 * ! Only views with content should have an NCurses Window structure..
 * ! Rewrote the view/window handling tossed out the old layout thingie
 */
#include <iostream>
#include <ncurses.h>
#include <string_view>
#include <vector>


#include "Core/macOS/MacOSKeyboardMonitor.h"

#include "Core/NCurses/NCursesScreen.h"
#include "Core/NCurses/NCursesKeyboardDriver.h"

#include "Core/Line.h"
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
#include "Core/NCurses/NCursesKeyboardDriver.h"
#include "Core/macOS/MacOSKeyboardMonitor.h"

#include "Core/BufferManager.h"
#include "Core/TextBuffer.h"

// Bring in the view handling
#include "Core/Views/ViewBase.h"
#include "Core/Views/GutterView.h"
#include "Core/Views/EditorView.h"
#include "Core/Views/RootView.h"
#include "Core/Views/CommandView.h"
#include "Core/Views/HSplitView.h"
#include "Core/Views/VSplitView.h"
#include "Core/Views/HStackView.h"
#include "Core/Views/VStackView.h"
#include "Core/Views/SingleLineView.h"


#include "logger.h"
#include <map>

using namespace gedit;

static MacOSKeyboardMonitor keyboardMonitor;
static NCursesKeyboardDriver keyboardDriver;

using namespace gedit;

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


static void SetupLogger() {
    char *sinkArgv[]={"autoflush","file","logfile.log"};
    gnilk::Logger::Initialize();
    auto fileSink = new gnilk::LogFileSink();
    gnilk::Logger::AddSink(fileSink, "fileSink", 3, sinkArgv);
    // Remove the console sink (it is auto-created in debug-mode)
    gnilk::Logger::RemoveSink("console");
}
extern char glbFillchar;

int main(int argc, const char **argv) {

    SetupLogger();
    auto logger = gnilk::Logger::GetLogger("main");


    logger->Debug("Loading configuration");
    auto configOk = Config::Instance().LoadConfig("config.yml");
    if (!configOk) {
        logger->Error("Unable to load default configuration from 'config.yml' - defaults will be used");
        exit(1);
    }

    logger->Debug("Configuring language parser(s)");
    CPPLanguage cppLanguage;
    cppLanguage.Initialize();
    Config::Instance().RegisterLanguage(".cpp", &cppLanguage);



    bool bQuit = false;
    NCursesScreen screen;

    if (!keyboardMonitor.Start()) {
        exit(1);
    }
    keyboardDriver.Begin(&keyboardMonitor);

    RuntimeConfig::Instance().SetScreen(screen);
    RuntimeConfig::Instance().SetKeyboard(keyboardDriver);

    logger->Debug("Initialize Graphics subsystem");

    screen.Open();
    screen.Clear();

    logger->Debug("Configuring colors and theme");
    // NOTE: This must be done after the screen has been opened as the color handling might require the underlying graphics
    //       context to be initialized...
    auto &colorConfig = Config::Instance().ColorConfiguration();
    for(int i=0;gnilk::IsLanguageTokenClass(i);i++) {
        auto langClass = gnilk::LanguageTokenClassToString(static_cast<kLanguageTokenClass>(i));
        if (!colorConfig.HasColor(langClass)) {
            logger->Warning("Missing color configuration for: %s", langClass.c_str());
            return -1;
        }
        screen.RegisterColor(i, colorConfig.GetColor(langClass), colorConfig.GetColor("background"));
    }



    logger->Debug("Creating views");

    auto dimensions = screen.Dimensions();

    logger->Debug("Dimensions (x,y): %d, %d", dimensions.Width(), dimensions.Height());

    RootView rootView;

    auto hSplitView = HSplitView(dimensions);
    rootView.AddView(&hSplitView);


    auto cmdView = CommandView();
    hSplitView.SetLower(&cmdView);

    auto vStackView = VStackView();
    auto headerView = SingleLineView();
    headerView.SetHeight(1);        // This is done in the SingleLineView

    hSplitView.SetUpper(&vStackView);

    auto hStackView = HStackView();
    auto gutterView = GutterView();
    gutterView.SetWidth(10);

    //EditorView editorView;
    auto editorView = EditorView();

    hStackView.AddSubView(&gutterView, kFixed);
    hStackView.AddSubView(&editorView, kFill);

    vStackView.AddSubView(&headerView, kFixed);
    vStackView.AddSubView(&hStackView, kFill);

    RuntimeConfig::Instance().SetRootView(&rootView);

    if (argc > 1) {
        logger->Debug("Loading file: %s", argv[1]);
        auto buffer = BufferManager::Instance().NewBufferFromFile(argv[1]);

        buffer->SetLanguage(Config::Instance().GetLanguageForFilename(argv[1]));

        editorView.GetEditController().SetTextBuffer(buffer);
    } else {
        auto buffer = BufferManager::Instance().NewBuffer("no_name");
        editorView.GetEditController().SetTextBuffer(buffer);
    }

    rootView.AddTopView(&editorView);
    rootView.AddTopView(&cmdView);

    rootView.Initialize();
    rootView.InvalidateAll();
    screen.Clear();
    rootView.Draw();
    screen.Update();
    refresh();
    rootView.Draw();
    screen.Update();


    // This is currently the run loop...
    while(!bQuit) {
        // This is way too simple - need better handling here!
        // Background stuff can cause need to repaint...
        //rootView.TopView()->GetWindow()->TestRefreshEx();
        bool redraw = false;
        glbFillchar = 'a';

        auto keyPress = keyboardDriver.GetKeyPress();
        if (keyPress.IsAnyValid()) {
            logger->Debug("KeyPress Valid - passing on...");
            rootView.TopView()->HandleKeyPress(keyPress);
            redraw = true;
        }
        if (rootView.IsInvalid()) {
            redraw = true;
        }
        if (redraw == true) {
            logger->Debug("Redraw was triggered...");
            rootView.Draw();
            screen.Update();
        }
    }
    logger->Debug("Left main loop, closing graphics subsystem");
    screen.Close();
    return 0;

    return -1;
}
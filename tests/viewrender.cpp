//
// Created by gnilk on 13.02.23.
//
/*
 * TO-DO List
 * - BaseController, handle key press (take from old ModeBase/EditorMode)
 * - Figure out how to handle 'HasContentChanged' notfications to force redraws..
 *   a) be in the redraw loop and just do it (let the views take care of it)
 *   b) Somehow let a controller or view set a flag that a redraw is needed..
 * - CommandController, NOTE: THIS IS QUITE THE TASK
 *   a) Make it on par with the old CommandMode
 *   b) Break-out and start implement CmdLet handling
 *   c) Define the proper API for talking to the editor through the cmd-let's
 * - Create a specific HSplitView which can support a 'split' window like feature and on-request resize
 *   both views (upper/lower) in tandem..
 *   Note: This requires ability for a view to define 'lock'/'anchor' points and also to define relative vs absolute
 *   layout handling (this is getting frustratingly similar to CSS)
 * - Create a 'StatusBar' view (Single line, no border)
 * - Create a VStackView, which simply 'stacks' and computes sizes accordingly when updated
 * - Import the language/color features in to this project
 * - Promote this project to the new 'main' project...
 * - BufferManager should store 'fullPathName' and 'name'
 * - Unsaved file should have '*' marking in the top..
 *
 *
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


#include "logger.h"
#include <map>

static MacOSKeyboardMonitor keyboardMonitor;
static gedit::NCursesKeyboardDriverNew keyboardDriver;

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


static void loadBuffer(const char *filename, EditorMode &editorMode) {
    auto logger = gnilk::Logger::GetLogger("loader");

    Buffer *buffer = new Buffer();
    logger->Debug("Loading file given from cmd-line: %s", filename);

    if (!LoadToBuffer(*buffer, filename)) {
        logger->Error("Unable to load: %s", filename);
        exit(1);
    }
    buffer->SetLanguage(Config::Instance().GetLanguageForFilename(filename));

    logger->Debug("Ok, file loaded (line: %d)", (int)buffer->Lines().size());
    logger->Debug("Assigning buffer");
    editorMode.SetBuffer(buffer);
}

static void SetupLogger() {
    char *sinkArgv[]={"autoflush","file","logfile.log"};
    gnilk::Logger::Initialize();
    auto fileSink = new gnilk::LogFileSink();
    gnilk::Logger::AddSink(fileSink, "fileSink", 3, sinkArgv);
    // Remove the console sink (it is auto-created in debug-mode)
    gnilk::Logger::RemoveSink("console");
}


int main(int argc, const char **argv) {

    SetupLogger();
    auto logger = gnilk::Logger::GetLogger("main");

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

    logger->Debug("Entering mainloop");

    auto dimensions = screen.Dimensions();

    logger->Debug("Dimensions (x,y): %d, %d", dimensions.Width(), dimensions.Height());

    RootView rootView;
    rootView.SetFlags(gedit::ViewBase::kViewNone);

    // The splitter holds Editor (upper) and Cmd (lower)
    HSplitView hSplitView;
    hSplitView.SetFlags(gedit::ViewBase::kViewNone);
    rootView.AddView(&hSplitView);

    VSplitView vSplitView(Rect(dimensions.Width(), dimensions.Height() * 0.7));
    vSplitView.SetFlags(gedit::ViewBase::kViewNone);
    hSplitView.SetTopView(&vSplitView);

    CommandView commandView;
    commandView.SetCaption("CmdView");
    hSplitView.SetBottomView(&commandView);

    GutterView gutterView;
    gutterView.SetFlags((ViewBase::kViewFlags ) (ViewBase::kViewDrawLeftBorder | ViewBase::kViewDrawUpperBorder | ViewBase::kViewDrawLowerBorder));
    vSplitView.SetLeftView(&gutterView);

    EditorView editorView;
    editorView.SetCaption("Editor");
    vSplitView.SetRightView(&editorView);


    //RuntimeConfig::Instance().SetRootView(&editorView);
    RuntimeConfig::Instance().SetRootView(&rootView);

    rootView.ComputeInitialLayout(Rect(dimensions.Width()-1, dimensions.Height()-1));

    // Kick off the whole thing..
    rootView.Begin();



    auto buffer = BufferManager::Instance().NewBufferFromFile("test_src2.cpp");
    editorView.GetEditController().SetTextBuffer(buffer);

    rootView.AddTopView(&editorView);
    rootView.AddTopView(&commandView);


    // Initial draw - draw everything...
    rootView.InvalidateAll();
    screen.BeginRefreshCycle();
    rootView.Draw();
    screen.EndRefreshCycle();

    // This is currently the run loop...
    while(!bQuit) {
        // This is way too simple - need better handling here!
        // Background stuff can cause need to repaint...
        auto keyPress = keyboardDriver.GetKeyPress();

        rootView.TopView()->OnKeyPress(keyPress);
        screen.BeginRefreshCycle();
        rootView.Draw();
        screen.EndRefreshCycle();
        //screen.Update();
        if (screen.IsSizeChanged(true)) {
            screen.Clear();
        }
    }
    logger->Debug("Left main loop, closing graphics subsystem");
    screen.Close();
    return 0;

    return -1;
}
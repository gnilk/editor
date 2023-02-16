//
// Created by gnilk on 13.02.23.
//
#include <iostream>
#include <ncurses.h>
#include <string_view>
#include <vector>


#include "Core/macOS/MacOSKeyboardMonitor.h"

#include "Core/NCurses/NCursesScreen.h"
#include "Core/NCurses/NCursesKeyboardDriver.h"
#include "Core/ViewBase.h"

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

#include "Core/GutterView.h"
#include "Core/EditorView.h"
#include "Core/BufferManager.h"
#include "Core/RootView.h"
#include "Core/CommandView.h"
#include "Core/TextBuffer.h"

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

    gedit::Rect baseRect(dimensions.Width()-1, dimensions.Height()-1);
    gedit::RootView rootView(baseRect);
    // Disable any drawing on updates
    rootView.SetFlags(gedit::ViewBase::kViewNone);

    // TODO: We can reverse the creation process...
    gedit::Rect rectUpperLayoutView(baseRect);
    rectUpperLayoutView.SetHeight(2 * baseRect.Height()/3);
    rectUpperLayoutView.Move(0,0);
    gedit::LayoutView viewUpperLayout(rectUpperLayoutView);
    rootView.AddView(&viewUpperLayout);

    // The gutter
    gedit::Rect rectGutter(rectUpperLayoutView);
    rectGutter.SetWidth(6);
    gedit::GutterView gutterView(rectGutter);
    viewUpperLayout.AddView(&gutterView);

    // The editor
    gedit::Rect rectEditor(rectUpperLayoutView);
    rectEditor.SetWidth(rectUpperLayoutView.Width()-6);
    rectEditor.Move(6,0);
    gedit::EditorView editorView(rectEditor);
    editorView.SetCaption("Editor");
    viewUpperLayout.AddView(&editorView);

    // Setup the command view...
    gedit::Rect commandViewRect(baseRect);
    commandViewRect.SetHeight(1 + baseRect.Height()/3);
    commandViewRect.Move(0,2 * baseRect.Height()/3);
    gedit::CommandView commandView(commandViewRect);
    commandView.SetCaption("CommandView");
    rootView.AddView(&commandView);

    //RuntimeConfig::Instance().SetRootView(&editorView);
    RuntimeConfig::Instance().SetRootView(&rootView);

    // Kick off the whole thing..
    rootView.Begin();

    //loadBuffer("test_src2.cpp", editorView.GetEditorController());
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
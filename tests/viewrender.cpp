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

#include "Core/EditorView.h"

#include "logger.h"
#include <map>

static MacOSKeyboardMonitor keyboardMonitor;
static gedit::NCursesKeyboardDriverNew keyboardDriver;

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
    gedit::ViewBase rootView(baseRect);
    // Disable any drawing on updates
    rootView.SetFlags(gedit::ViewBase::kViewNone);

    // Editor view
    gedit::Rect subViewRect1(baseRect);
    subViewRect1.SetHeight(2 * baseRect.Height()/3);
    subViewRect1.Move(0,0);
    gedit::EditorView editorView(subViewRect1);
    editorView.SetCaption("Editor");
    rootView.AddView(&editorView);

    gedit::Rect subViewRect2(baseRect);
    subViewRect2.SetHeight(1 + baseRect.Height()/3);
    subViewRect2.Move(0,2 * baseRect.Height()/3);
    gedit::ViewBase subView2(subViewRect2);
    subView2.SetCaption("CommandView");
    rootView.AddView(&subView2);

    RuntimeConfig::Instance().SetRootView(&editorView);

    editorView.Begin();

    loadBuffer("test_src2.cpp", editorView.GetEditorController());


    // This is currently the run loop...
    while(!bQuit) {
        auto keyPress = keyboardDriver.GetKeyPress();
        editorView.OnKeyPress(keyPress);
        //rootView.OnKeyPress(keyPress);
        rootView.Draw();
        screen.Update();
        if (screen.IsSizeChanged(true)) {
            screen.Clear();
        }
    }
    logger->Debug("Left main loop, closing graphics subsystem");
    screen.Close();
    return 0;

    return -1;
}
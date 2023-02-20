//
// Created by gnilk on 20.02.23.
//
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

using namespace gedit;

static MacOSKeyboardMonitor keyboardMonitor;
static gedit::NCursesKeyboardDriverNew keyboardDriver;

static void SetupLogger() {
    char *sinkArgv[]={"autoflush","file","logfile.log"};
    gnilk::Logger::Initialize();
    auto fileSink = new gnilk::LogFileSink();
    gnilk::Logger::AddSink(fileSink, "fileSink", 3, sinkArgv);
    // Remove the console sink (it is auto-created in debug-mode)
    gnilk::Logger::RemoveSink("console");
}


int main(int argc, char **argv) {
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
    rootView.SetCaption("EditorView");
    rootView.SetFlags(gedit::ViewBase::kViewNone);

    rootView.ComputeInitialLayout(Rect(dimensions.Width()-1, dimensions.Height()-1));
    rootView.Begin();
    //rootView.InvalidateAll();

    // In order for scrolling to work the output must be associated with the native-window
    auto dc = rootView.ContentAreaDrawContext();
    for(int i=0;i<10;i++) {
        char buffer[64];
        snprintf(buffer, 64, "line %d", i);
        //mvwprintw(win, i,0, buffer);
        // mvprintw(i,0, buffer);
        dc->DrawStringAt(0,i, buffer);
    }

//    auto buffer = BufferManager::Instance().NewBufferFromFile("test_src2.cpp");
//    rootView.GetEditController().SetTextBuffer(buffer);
    while(!bQuit) {
        auto keyPress = keyboardDriver.GetKeyPress();
        if (keyPress.key == kKey_Down) {
            rootView.GetNativeWindow()->Scroll(1);
        }

//        screen.BeginRefreshCycle();
        rootView.Draw();
//        screen.EndRefreshCycle();
        //screen.Update();
//        if (screen.IsSizeChanged(true)) {
//            screen.Clear();
//        }
    }


    return -1;
}
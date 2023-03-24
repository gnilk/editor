//
// Created by gnilk on 13.02.23.
//
/*
 * TO-DO List
 * 1)
 * - Make some classes thread aware (TextBuffer / Line class - perhaps most important)
 * - Ability to push events (with data) from one thread to the main thread...
 *   These events should be executed _BEFORE_ any keyhandling is done..
 *   Note: This would make it nicer with an event based UI.....
 *
 * 2)
 * - CommandView, Store/Restore splitter when view goes inactive/active
 *   Note: this can be tested before adjusting on new line
 * - CommandView should adjust height of splitter on new lines..
 *   note: once done, remove f1/f2 adjustment keys...
 *
 * Done:
 * ! New CompositionObject between View/Controller/Data => EditorModel
 *      ! Should hold an EditController, TextBuffer and ViewData
 *      ! Change the way EditView works, instead of owning the controller - the controller is set
 *      ! RuntimeConfiguration should have a function to retrieve the active EditorModel
 *      ! Move Cursor to EditorModel instance!!
 * ! In BufferManager - make it possible to iterate through all buffers currently open..
 * ! headerView -> Specialize SingleLineView to 'HeaderView' make the draw function
 * ! new single view for status line / splitter -> make this a specific "HSplitView"
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
#include "Core/KeyMapping.h"

#include "Core/Language/CPP/CPPLanguage.h"
#include "Core/Language/LangToken.h"


#include "Core/RuntimeConfig.h"
#include "Core/Buffer.h"
#include "Core/NCurses/NCursesKeyboardDriver.h"
#include "Core/macOS/MacOSKeyboardMonitor.h"

#include "Core/BufferManager.h"
#include "Core/TextBuffer.h"

#include "Core/Editor.h"

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
#include "Core/Views/HeaderView.h"
#include "Core/Views/HSplitViewStatus.h"

#include "Core/Action.h"


#include "logger.h"
#include <map>

using namespace gedit;


static void SetupLogger() {
}
extern char glbFillchar;

int main(int argc, const char **argv) {

    Editor::Instance().Initialize(argc, argv);

    // Note: This can be implicit
    RuntimeConfig::Instance().SetMainThreadID();

    auto logger = gnilk::Logger::GetLogger("main");
    bool bQuit = false;

    auto screen = RuntimeConfig::Instance().Screen();
    auto keyboardDriver = RuntimeConfig::Instance().Keyboard();
    auto dimensions = screen->Dimensions();

    auto models = Editor::Instance().GetModels();
    for(auto m : models) {
        logger->Debug("File: %s",m->GetTextBuffer()->Name().c_str());
    }


    logger->Debug("Creating views");
    logger->Debug("Dimensions (x,y): %d, %d", dimensions.Width(), dimensions.Height());

    RootView rootView;

    auto hSplitView = HSplitViewStatus(dimensions);
    rootView.AddView(&hSplitView);


    auto cmdView = CommandView();
    hSplitView.SetLower(&cmdView);

    auto vStackView = VStackView();
    auto headerView = HeaderView();
    headerView.SetHeight(1);        // This is done in the SingleLineView

    hSplitView.SetUpper(&vStackView);

    auto hStackView = HStackView();
    auto gutterView = GutterView();
    gutterView.SetWidth(10);

    auto editorView = EditorView();

    hStackView.AddSubView(&gutterView, kFixed);
    hStackView.AddSubView(&editorView, kFill);

    vStackView.AddSubView(&headerView, kFixed);
    vStackView.AddSubView(&hStackView, kFill);

    RuntimeConfig::Instance().SetRootView(&rootView);

    rootView.AddTopView(&editorView);
    rootView.AddTopView(&cmdView);

    rootView.Initialize();
    rootView.InvalidateAll();
    screen->Clear();
    rootView.Draw();
    screen->Update();
    refresh();
    rootView.Draw();
    screen->Update();


    // This is currently the run loop...
    while(!bQuit) {
        // This is way too simple - need better handling here!
        // Background stuff can cause need to repaint...
        //rootView.TopView()->GetWindow()->TestRefreshEx();

        // Process any messages from other threads before we do anything else..
        bool redraw = false;

        if (rootView.ProcessMessageQueue() > 0) {
            redraw = true;
        }


        glbFillchar = 'a';

        auto keyPress = keyboardDriver->GetKeyPress();
        if (keyPress.IsAnyValid()) {

            logger->Debug("KeyPress Valid - passing on...");

            auto action = KeyMapping::Instance().ActionFromKeyPress(keyPress);
            if (action != gedit::kAction::kActionNone) {
                logger->Debug("Action found - sending to RootView");
                rootView.OnAction(action);
            } else {
                logger->Debug("No action for keypress, treating as regular input");
                rootView.TopView()->HandleKeyPress(keyPress);
            }
            redraw = true;
        }


        if (rootView.IsInvalid()) {
            redraw = true;
        }
        if (redraw == true) {
            logger->Debug("Redraw was triggered...");
            rootView.Draw();
            screen->Update();
            refresh();
        }
    }
    logger->Debug("Left main loop, closing graphics subsystem");
    screen->Close();
    return 0;

    return -1;
}
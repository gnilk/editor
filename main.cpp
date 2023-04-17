//
// Created by gnilk on 13.02.23.
//
/*
 * TO-DO List
 * - Swap out the vertical navigation code in EditorView for the 'VerticalNavigationModel'
 * - Make a tree-list-view (for files and such)
 * - Test if the underlying VStack view (or editor view) can have a popup-one/two liner (Search)
 * - Embryo for what is needed for the API lies within the ListSelectionModal, example:
 *   - List/Select/Switch buffer (Popup with list of active buffers)
 *   - Open file
 *   - etc..
 * - Create a view base class 'Visible' / 'Drawable' - View, which contain the setup code found in "ListSelectionModal'
 * ! Move render loop out of here, we need control over it in case we want to display modal dialogs.
 * - Make large files > 10k lines read-only, alt. disable reparsing and syntax highlighting for large files..
 *   Disabling syntax can be deduced on-the fly by measuring the reparsing process..
 * ! SDL2 backend, SDL3 is way too instable (no Linux support) so we need another one...
 * ! Speed up tokenizer in editor - consider putting in on a background thread...
 *   Consider making ReparseLine a function to use.
 *   Add a marker (like stack-size from parser) to each line, an intelligent reparse function
 *   would search (from current line) backwards to the first line where the stack marker = 0
 *   which would indicate a clean state for the parser... We probably would need a cut-off as well
 *   in which the Reparse function spawns a background thread to update the full buffer...
 *   Note: The above won't work cleanly - assume we put a block comment at the top and then remove it => whole buffer reparsing
 *   Note2: It actually might work - but in case you start putting a comment in the beginning I must reparse the whole file..
 * ! Properly quit editor through API
 * ! Make some classes thread aware (TextBuffer / Line class - perhaps most important)
 * ! Fix NCurses, currently broken (due to work on SDL3 backend)
 *
 * Done:
 * ! Handling of overlay's or 'selection marking', added special function for drawing overlays for a specific line
 * ! Refactor the Action parser (KeyMapping.cpp) so that Action = Keymapp
 *   ! Add 'optional' (for modifiers) on action
 *   ! Make the optional a lookup:
 *     set SelectionModifier : KeyCode_Shift
 *     NavigateLineDown : KeyCode_DownArrow + @SelectionModifier
 * ! CommandView, Store/Restore splitter when view goes inactive/active
 *   Note: this can be tested before adjusting on new line
 * ! CommandView should adjust height of splitter on new lines..
 *   note: once done, remove f1/f2 adjustment keys...
 *
 * ! Ability to push events (with data) from one thread to the main thread...
 *   These events should be executed _BEFORE_ any keyhandling is done..
 *   Note: This would make it nicer with an event based UI.....
 *
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
 * ! Consolidate NCursesKeyBoard kKeyCode_xxxx with Keyboard::kKeyCode - currently there is a mismatch..
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
 * ! Create a 'StatusBar' view (Single line, no border)
 * ! HStackView, which simply 'stacks' and computes sizes accordingly when updated
 * ! Import the language/color features in to this project
 * ! Promote this project to the new 'main' project...
 * - BufferManager should store 'fullPathName' and 'name'
 * - Unsaved file should have '*' marking in the top..
 * ! Consider relationship between viexw/context/window - right now there is too much flexibility
 * ! Only views with content should have an NCurses Window structure..
 * ! Rewrote the view/window handling tossed out the old layout thingie
 */
#include <SDL3/SDL_keyboard.h>
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
#include "Core/KeyMapping.h"
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
#include "Core/Runloop.h"

#include "logger.h"
#include <map>
#include "Core/API/EditorAPI.h"
#include "Core/Views/ModalView.h"
#include "Core/Views/ListSelectionModal.h"
#include "Core/Views/TreeSelectionModal.h"
using namespace gedit;


static void SetupLogger() {
}
extern char glbFillchar;

static void TestKeyBoardDriver() {
    auto screen = RuntimeConfig::Instance().Screen();
    auto keyboardDriver = RuntimeConfig::Instance().Keyboard();
    auto logger = gnilk::Logger::GetLogger("kbdtest");

    SDL_StartTextInput();

    bool bQuit = false;
    while(!bQuit) {
        auto kp = keyboardDriver->GetKeyPress();
        if (!kp.IsAnyValid()) {
            continue;
        }
        if ((kp.isSpecialKey) && (kp.specialKey == Keyboard::kKeyCode_Escape)) {
            bQuit = true;
            continue;
        }
        if (kp.isSpecialKey) {
            auto keyName = KeyMapping::Instance().KeyCodeName(static_cast<Keyboard::kKeyCode>(kp.specialKey));
            logger->Debug("special kp, modifiers=%.2x, specialKey=%.2x (%s)", kp.modifiers, kp.specialKey, keyName.c_str());
        } else {
            logger->Debug("kp, modifiers=%.2x, scancode=%.2x, key=%.2x (%c), isKey=%s", kp.modifiers, kp.hwEvent.scanCode, kp.key, kp.key, kp.isKeyValid?"yes":"no");
        }
        if (kp.IsAnyValid()) {

            logger->Debug("KeyPress Valid - checking actions");

            if (kp.isKeyValid) {
                int breakme = 0;
            }


            auto kpAction = KeyMapping::Instance().ActionFromKeyPress(kp);
            if (kpAction.has_value()) {
                logger->Debug("Action '%s' found - modifier: 0x%.2x (%s)",
                              KeyMapping::Instance().ActionName(kpAction->action).c_str(),
                              kpAction->modifierMask,
                              KeyMapping::Instance().ModifierName(*kpAction->modifier).c_str());
            }
        }
    }
    screen->Close();
    exit(1);

}




int main(int argc, const char **argv) {

    Editor::Instance().Initialize(argc, argv);

//    {
//        auto screen = RuntimeConfig::Instance().Screen();
//        screen->Close();
//        return -1;
//    }


    // Note: This can be implicit
    RuntimeConfig::Instance().SetMainThreadID();

    //TestKeyBoardDriver();


    auto logger = gnilk::Logger::GetLogger("main");

    auto screen = RuntimeConfig::Instance().Screen();
    auto dimensions = screen->Dimensions();

    auto models = Editor::Instance().GetModels();
    for(auto m : models) {
        logger->Debug("File: %s",m->GetTextBuffer()->Name().c_str());
    }

    //
    // This set's up the UI and configures all the views...
    //

    logger->Debug("Creating views");
    logger->Debug("Dimensions (x,y): %d, %d", dimensions.Width(), dimensions.Height());


    //
    // The views are configured like this; the number indicates the view depth/hierachy
    // Note: There is another HStackView for Editor+Gutter
    //
    //        [fixed]                 [fill]
    //  | 3) VStackView       |  3) VStackView
    //  |   4) Singleline     |    4) HeaderView [tabs]     <- fixed (1 line)
    //  |    <-------  2) HStackView  ------->
    //  |    4) TreeView      | <-- 4) HStackView -->           <- fill
    //  |                     | 5) Gutter | 5) Editor
    //  |                     |           |
    //  |                     |           |
    //  |                     |           |
    //  |                     |           |
    //  1) --------- HSplitViewStatus ----------------
    //  |                                           |
    //  |             2) CmdView                    |
    //  |                                           |
    RootView rootView;

    auto hSplitView = HSplitViewStatus(dimensions);
    rootView.AddView(&hSplitView);


    auto cmdView = CommandView();
    hSplitView.SetLower(&cmdView);

    auto vStackViewEditor = VStackView();   // This is where the editor and the 'FileHeader' lives, they are stacked vertically
    auto headerView = HeaderView();
    headerView.SetHeight(1);        // This is done in the SingleLineView


    auto vStackViewExplorer = VStackView();
    vStackViewExplorer.SetWidth(16);
    auto fileExplorerHeader = SingleLineView();
    auto fileExplorer = TreeView<std::string>::Create();
    fileExplorer->SetToStringDelegate([](const std::string &data) -> std::string {
        return data;
    });

    vStackViewExplorer.AddSubView(&fileExplorerHeader, kFixed);
    vStackViewExplorer.AddSubView(fileExplorer.get(), kFill);

    auto subNode = fileExplorer->AddItem("Item 1");
    auto subba = fileExplorer->AddItem(subNode, "Item 1:1");
    fileExplorer->AddItem(subba, "Item 1:2:1");
    fileExplorer->AddItem(subba, "Item 1:2:2");
    fileExplorer->AddItem(subNode, "Item 1:2");

    fileExplorer->AddItem("Item 2");
    subNode = fileExplorer->AddItem("Item 3");
    subba = fileExplorer->AddItem(subNode, "Item 3:1");
    subba = fileExplorer->AddItem(subNode, "Item 3:2");
    subba = fileExplorer->AddItem(subNode, "Item 3:3");
    fileExplorer->AddItem("Item 4");



    auto hStackViewUpper = HStackView();
    auto hStackViewEditor = HStackView();
    auto gutterView = GutterView();
    gutterView.SetWidth(10);

    auto editorView = EditorView();

    hStackViewEditor.AddSubView(&gutterView, kFixed);
    hStackViewEditor.AddSubView(&editorView, kFill);

    hStackViewUpper.AddSubView(&vStackViewExplorer, kFixed);
    hStackViewUpper.AddSubView(&vStackViewEditor, kFill);

    vStackViewEditor.AddSubView(&headerView, kFixed);
    vStackViewEditor.AddSubView(&hStackViewEditor, kFill);

    RuntimeConfig::Instance().SetRootView(&rootView);

    hSplitView.SetUpper(&hStackViewUpper);

    rootView.AddTopView(&editorView);
    rootView.AddTopView(&cmdView);

//    TreeSelectionModal<std::string> myModal;
//    myModal.GetTree()->SetToStringDelegate([](const std::string &data) -> std::string {
//       return data;
//    });
//
//    auto tree = myModal.GetTree();
//    auto subNode = tree->AddItem("Item 1");
//    auto subba = tree->AddItem(subNode, "Item 1:1");
//    tree->AddItem(subba, "Item 1:2:1");
//    tree->AddItem(subba, "Item 1:2:2");
//    tree->AddItem(subNode, "Item 1:2");
//
//    tree->AddItem("Item 2");
//    subNode = tree->AddItem("Item 3");
//    subba = tree->AddItem(subNode, "Item 3:1");
//    subba = tree->AddItem(subNode, "Item 3:2");
//    subba = tree->AddItem(subNode, "Item 3:3");
//    tree->AddItem("Item 4");

//    ListSelectionModal myModal;
//    char buffer[128];
//    for(int i=0;i<64;i++) {
//        snprintf(buffer, 128, "Item %d", i);
//        myModal.AddItem(buffer);
//    }
//    Runloop::ShowModal(&myModal);

    rootView.Initialize();
    rootView.InvalidateAll();

    // No clue why I have to do this twice - but otherwise it doesn't work...
    screen->Clear();
    rootView.Draw();
    screen->Update();

    screen->Clear();
    rootView.Draw();
    screen->Update();

    //
    // Once done, we just run the main loop
    //

    Runloop::DefaultLoop();
    logger->Debug("Left main loop, closing graphics subsystem");
    screen->Close();
    return 0;
}
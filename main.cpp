//
// Created by gnilk on 13.02.23.
//
/*
 * TO-DO List
 * - Merge shell and 'Editor' bootstrap script capabilities and move to CommandController
 * - Fix save!
 * - Code cleanup
 *   - Switch to References everywhere, either C++ ref or shared_ptr type of refs (MyClass::Ref), there are still too many places using raw object pointers
 * - QuickCommandMode
 *   ! Allow cursor to be positioned at the 'C' input
 *   ! Ability to navigate through search results (next/previous) - should reposition the cursor
 *   ! Consider how to visualize various things coming from the quick-command mode
 *     ! you are in search and want 'hits', perhaps change 'C' to 'S' in the prefix??
 *   ! Require prefix '.' before entering commands (need states) as commands require bypassing short-cuts..
 *   ! When '.' is entered we should disable ASCII commands (like we do for search) otherwise certain key-combos are not valid!
 *     Illustrate: press './' and you will enter search... this should not be the case..
 *   - How to search in node-editor mode (i.e ProjectViews, Terminal - history)
 *   - Movement (see Helix editor)
 * - Bookmarks
 *   - visual (GutterView support)
 *   - Add/Delete
 *   - Jump (Next/Previous)
 * - Rename actions (see Helix editor)
 * - Make something like 'Telescope' (Neovim, Hybrid)
 * - Intellisense
 *    - Build and internal prefix-tree DB
 *    - Be 'smart' depending on language search for type belonging to related files (C/CPP - header files)
 *    - Make sure the prefix-tree can be quickly update when changing stuff on a line
 *    - Make the Intellisense run in a background thread that locks the whole textbuffer (but does so when it sits Idle)
 * + Make a proper 'project' viewer
 *   ! Enter on selection should load (if needed) and set the model active in the editor
 *   - Ability to search/filter for files
 * - Consolidate configuration (or sort it up properly) currently spread out beween 'Editor', 'Config', 'RuntimeConfig'
 * - Put some performance timings in the LanguageParser (this will have to be optimized sooner or later)
 * - Something is causing sigsev, I strongly suspect that it is related to threading in Syntax Coloring / Language code
 * - macOS swaps left/right scancodes between keyboards (laptop has left/right one way my ext.keyboard another)
 *   need to consider a solution for this...
 * - Swap out the vertical navigation code in EditorView for the 'VerticalNavigationModel'
 * - Make large files > 10k lines read-only, alt. disable reparsing and syntax highlighting for large files..
 *   Disabling syntax can be deduced on-the fly by measuring the reparsing process..
 * - Unsaved file should have '*' marking in the top..
 * - Remove the buffer manager class - not needed
 *
 * Done:
 * ! Switching views should leave quickcommand mode if in it..
 * ! Workspace, need to separate name from which path (default should be '.')
 * ! Add view-visibility to config (should be possible to remove project-viewer)
 * ! Might need to rewrite NCurses like SDL - not using the underlying 'windowing' mechanism but rather reposition everything myself
 * ! SDL2 on par with SDL3
 * ! [deprecated] BufferManager should store 'fullPathName' and 'name'
 * ! Move EditorController functionality to EditorModel
 * ! Make API register commands in the runtime configuration
 *   ! Move 'PluginCommand' class from JSEngine/JSPluginCommand to Core/Plugin/
 *   ! Create a 'PluginExecutor' and associate with each PluginCommand
 * ! Start with API work
 *   ! Goal: Javascript function to associate the current buffer with a specific language parser
 *   ! API requires:
 *     ! list installed languages; <array> EditorAPI::GetInstalledLanguages();
 *     ! get a language reference; <LanguageRef> EditorAPI::GetLanguage(<str_identifier>);
 *     ! get current buffer; <TextBufferReference> EditorAPI::GetCurrentTextBuffer();
 *     !associate language reference with buffer; TextBufferAPI::SetLanguage(<LanguageRef>)
 * ! Add action to 'Comment Selection'
 * ! Move language parser files to other directory (currently in 'cpp' should be 'languages' or something)
 * ! Add an 'Array' kind of block definition to the language token specification
 * ! Make TextBuffer work with shared_ptr<Line> instead of raw Line *
 * ! Make 'CycleActiveView' a left/right function and go through the list of view which can have focus...
 * ! colors, need to fix the color handling!!!
 *   ! Make everything work with ColorRGBA
 *   ! Let the driver handle caching and mapping to it's internal structures
 *   ! Add a hash function to the ColorRGBA
 * ! Language tokenizer should use kBlockCodeStart/End to track indent
 * ! Views should have an option saying if they can have focus, make this a flags
 *   [note: Already supported, use 'AddTopView' and it will work]
 * ! Make a tree-list-view (for files and such)
 * ! Test if the underlying VStack view (or editor view) can have a popup-one/two liner (Search)
 * ! Create a view base class 'Visible' / 'Drawable' - View, which contain the setup code found in "ListSelectionModal'
 * ! Move render loop out of here, we need control over it in case we want to display modal dialogs.
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
 * ! Consider relationship between viexw/context/window - right now there is too much flexibility
 * ! Only views with content should have an NCurses Window structure..
 * ! Rewrote the view/window handling tossed out the old layout thingie
 */
#include <SDL3/SDL_keyboard.h>
#include <vector>


#include "Core/Editor.h"
#include "Core/KeyMapping.h"
#include "Core/Runloop.h"
#include "Core/StrUtil.h"
#include "Core/Keyboard.h"
#include "logger.h"

#include "Core/RuntimeConfig.h"

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
#include "Core/Views/EditorHeaderView.h"
#include "Core/Views/HSplitViewStatus.h"
#include "Core/Views/WorkspaceView.h"

#include "Core/Views/TreeSelectionModal.h"
#include "Core/Views/TestView.h"

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
            auto keyName = Keyboard::KeyCodeName(static_cast<Keyboard::kKeyCode>(kp.specialKey));
            logger->Debug("special kp, modifiers=%.2x, specialKey=%.2x (%s)", kp.modifiers, kp.specialKey, keyName.c_str());
        } else {
            logger->Debug("kp, modifiers=%.2x, scancode=%.2x, key=%.2x (%c), isKey=%s", kp.modifiers, kp.hwEvent.scanCode, kp.key, kp.key, kp.isKeyValid?"yes":"no");
        }
        if (kp.IsAnyValid()) {

            logger->Debug("KeyPress Valid - checking actions");

            auto &keyMap = Editor::Instance().GetActiveKeyMap();
            auto kpAction = keyMap.ActionFromKeyPress(kp);
            if (kpAction.has_value()) {
                logger->Debug("Action '%s' found - modifier: 0x%.2x (%s)",
                              keyMap.ActionName(kpAction->action).c_str(),
                              kpAction->modifierMask,
                              keyMap.ModifierName(*kpAction->actionModifier).c_str());
            }
        }
    }
    screen->Close();
    exit(1);

}
static void TestViewDrawing() {
    TestView testView(Rect(Point(10,10),40,30));
    //TestView testView;
    auto screen = RuntimeConfig::Instance().Screen();


    testView.Initialize();
    testView.InvalidateAll();
    RuntimeConfig::Instance().SetRootView(&testView);

    screen->Clear();
    testView.Draw();
    screen->Update();

    screen->Clear();
    testView.Draw();
    screen->Update();

    Runloop::DefaultLoop();
    //Runloop::TestLoop();
}




int main(int argc, const char **argv) {

    Editor::Instance().Initialize(argc, argv);
    Editor::Instance().OpenScreen();

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
        logger->Debug("File: %s",m->GetTextBuffer()->GetName().c_str());
    }

    //
    // This set's up the UI and configures all the views...
    //

    logger->Debug("Creating views");
    logger->Debug("Dimensions (x,y): %d, %d", dimensions.Width(), dimensions.Height());

//    TestViewDrawing();
//    exit(1);

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

    HSplitViewStatus hSplitViewStatus;
    rootView.AddView(&hSplitViewStatus);


    auto cmdView = CommandView();
    hSplitViewStatus.SetLower(&cmdView);

    auto vStackViewEditor = VStackView();   // This is where the editor and the 'FileHeader' lives, they are stacked vertically
    auto editorHeaderView = EditorHeaderView();
    editorHeaderView.SetHeight(1);        // This is done in the SingleLineView


    auto vStackViewWorkspace = VStackView();
    vStackViewWorkspace.SetWidth(24);
    auto workspaceHeader = SingleLineView();
    workspaceHeader.SetText("Workspace");
    auto workspaceExplorer = WorkspaceView();
//    workspaceExplorer->SetToStringDelegate([](const std::string &data) -> std::string {
//        return data;
//    });

    vStackViewWorkspace.AddSubView(&workspaceHeader, kFixed);
    vStackViewWorkspace.AddSubView(&workspaceExplorer, kFill);

//    auto subNode = workspaceExplorer->AddItem("Item 1");
//    auto subba = workspaceExplorer->AddItem(subNode, "Item 1:1");
//    workspaceExplorer->AddItem(subba, "Item 1:2:1");
//    workspaceExplorer->AddItem(subba, "Item 1:2:2");
//    workspaceExplorer->AddItem(subNode, "Item 1:2");
//
//    workspaceExplorer->AddItem("Item 2");
//    subNode = workspaceExplorer->AddItem("Item 3");
//    subba = workspaceExplorer->AddItem(subNode, "Item 3:1");
//    subba = workspaceExplorer->AddItem(subNode, "Item 3:2");
//    subba = workspaceExplorer->AddItem(subNode, "Item 3:3");
//    workspaceExplorer->AddItem("Item 4");



    auto hStackViewUpper = HStackView();
    auto hStackViewEditor = HStackView();
    auto gutterView = GutterView();
    gutterView.SetWidth(10);

    auto editorView = EditorView();

    hStackViewEditor.AddSubView(&gutterView, kFixed);
    hStackViewEditor.AddSubView(&editorView, kFill);

    hStackViewUpper.AddSubView(&vStackViewWorkspace, kFixed);
    hStackViewUpper.AddSubView(&vStackViewEditor, kFill);

    vStackViewEditor.AddSubView(&editorHeaderView, kFixed);
    vStackViewEditor.AddSubView(&hStackViewEditor, kFill);


    hSplitViewStatus.SetUpper(&hStackViewUpper);

    rootView.AddTopView(&editorView, "EditorView");
    rootView.AddTopView(&cmdView, "CommandView");
    rootView.AddTopView(&workspaceExplorer, "ProjectView");

//    WorkspaceView workspaceView;
//    ModalView dummy(dimensions, &workspaceView);
//    Runloop::ShowModal(&dummy);
//    exit(1);

    RuntimeConfig::Instance().SetRootView(&rootView);
    RuntimeConfig::Instance().SetQuickCmdView(&hSplitViewStatus);

    rootView.Initialize();
    rootView.InvalidateAll();

    Editor::Instance().RunPostInitalizationScript();

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

    Editor::Instance().Close();

    logger->Debug("Left main loop, closing graphics subsystem");
    screen->Close();
    return 0;
}
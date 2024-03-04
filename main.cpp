//
// Created by gnilk on 13.02.23.
//
/*
 * TO-DO List
 * + Remove 'kLanguageTokenClass' from 'LineAttrib' - this should better be tied to hint about rendering..
 * - Keymap: Add 'inherit' parameter so we can have a global definition of keymappings (like the UI keymappings)
 * - WorkspaceView - Home/End/PageUp/PageDown
 * + Rewrite 'CommandView' - replace with a 'TerminalView' which operates properly with the new shell component
 *   Should treat the shell as a stream rather than trying to keep track of cursor stuff and so forth..
 *   - Properly trap signals to detect if someone does 'exit' from shell - respawn shell in that case..
 * - Replace the language parser with the new Lexer from the AST project...
 * - Move as much out from EditorView/CommandView/QuickView/WorkspaceView as possible and put in resp. controller
 * - Sometimes loose syntax highlight - mostly seen towards end-of-file, need some 'reparse all' functionality
 *   or simply to use 'reparse-all' for any file < 1000 lines...
 * + Spotted another exception related to timers - but I think that was CPP-mode line bug
 * + Vertical navigation yet-again is acting strange on clipping when at the end of a file
 * - Delete some lines (upper 1/3 of file) and then page-down => segfault
 *   => Seen once??
 * - Figure something to handle 'tab' correctly
 *   should be in rendering - the editor should NOT modify unless the user tells it
 *   "forward cursor X" when \t occurs (in this case it should be part of the tokenizer)
 * + Exclude/Ignore directories for Monitor is a must
 *   Introduce a 'FolderMonitor' section in the config, should have 'Enable', 'Exclude'-list (glob-patterns)
 * - Introduce some delay in the monitoring allowing for add/remove before we refresh the editor
     Monitoring a path that changes quickly (like the build directory) will cause seg-faults
 * + There are segfaults in copy/paste
 * - Adding an additional ')' when the previous char is '(' should be ignored, typing: '(',')' inserts an extra ')'
 * + Revisit the 'Workspace::NewModel' and friends - there are too much similarity in these functions
 * - Save screen position and size upon resize/move and similar, restore on startup (use XDG state directory)
 * - Expose config from JS (set,get,list)
 *   Would be cool to just open the whole config folder as a workspace node..  <- consider this
 * - Ability to influence logger from JS (log enable xyz, log disable xyz, etc..)
 * - Notification system needs review, should be able to have multiple subscribers
 * - File monitoring and reloading (for theme's and other?)
 * + Define proper keymap for Linux (selection, copy/paste, etc..)
 * - LanguageToken to ScreenColor mapping is right now done in the editor - not sure where this should be..
 * + Fix save!
 * - How to search in node-editor mode (i.e ProjectViews, Terminal - history), also for quickmode?
 * - Swap out the vertical navigation code in EditorView for the 'VerticalNavigationModel'
 *
 * Bigger features:
 * - Need new UI => Take what we learned and incorporate
 * - Allow editor view to be a HexView
 * - Make something to hold a 'builder' (I need somewhere to store build-errors and present them nicely)
 *   Later this can go into the project configuration, which is executed through a '.build'-command
 * - Workspace configuration
 *   When opening a folder it should be possible to 'initialize' a workspace there - this workspace can hold specific
 *   settings for it - like folders for intellisense and other things (build params, run params, etc...)
 * - Add some tool awareness (like ability to jump to src:line when compiling and so forth)
 *   Either through some kind of build command which can attach parsers to the console..
 * - Use references in view-system (most other code using references or smart pointers)
 * - QuickMode support for Movement (see Helix editor)
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
 * - Put some performance timings in the LanguageParser (this will have to be optimized sooner or later)
 * - macOS swaps left/right scancodes between keyboards (laptop has left/right one way my ext.keyboard another)
 *   need to consider a solution for this...
 *
 * Done:
 * ! When the rootview 'SetActiveTopViewByName' set's same as currently active we loose it (input is gone)
 *        There is a workaround in root view which doesn't allow this - but still...
 * ! Undo does not properly reparse the area of the re-pasted data
 * ! Paste from external only works first time, then it is always the same buffer
 * ! Fan's go bananas and CPU is max on the editor CPU...
 *   ! Rewrite 'TimerController', calculate time-to-next and 'sleep' that interval...
 * [!] Not seen anymore: Large(?) files issue, after searching for an item and jumping to next a couple of times - scrolling up doesn't properly reposition view (need to scroll down first)
 * ! Delete a selection which start's at X (col) > 0 and ends on another line with col == 0 will remove one line too much
 * ! Spotted exception when in an empty file typing a line (CPP) mode ending with {} and pressing enter
 *   [2023-10-18, gnilk] I think this was fixed by removing the use of iterators and instead working with index
 * ! Undo does almost work
 * ! Auto save, add timer 'on change' and call 'save' when it expired - reset timer on every change..
 * ! Searching, searching for an item occuring only once (like the function name) doesnt jump to hit!
 * ! Language tokenizer has problem - keywords are found within other words...
 *   Keywords, known-types MUST BE classified AFTER a token has been extracted!!! -> Need ability to create whole-token identifiers
 * ! When creating a new model we should switch to it, also - it is created in the wrong folder..
 * ! Reparsing after delete is not working
 * ! (macos) CMD-End (nav-end-of-file) moves view but doesn't update internal cursor position (any other keystroke moves back)
 * ! Page-down, start selection (top section) there will be multiple selections
 * ! Switching active model/view doesn't update viewTopLine properly?
 *   => There is a screwup in who owns the 'idxActiveLine', view-top/bottom and cursor - MUST FIX!!
 * ! Terminal must forward stderr!!
 * ! Language parser should operate on U32
 * ! Change loader to convert everything to UTF32 - for fast rendering
 * ! Need a new text renderer -> handling of unicode
 * ! Cursor sometimes indicates wrong line but gutter-indicator seems correct!
 * ! Select/Copy/Paste are wrong - scrolling makes them non-reliable...
 * ! File monitoring on Linux
 * ! Selecting in workspace view does not change to editor-view
 * ! Scroll beyond the view-area and then swap between Project/Edit-View resets the file-view
 *   This happens when leaving the EditorView
 * ! Select and Delete outside the initial viewing area (height of window) makes the cursor disappear
 * ! Is 'Cut' implemented?
 * ! When creating a new model we should switch to it
 * ! WorkspaceView should preserve node expand/collapse information when rebuilding the tree...
 * ! WorkspaceView should react on changes from the Workspace::Desktop foldermonitor detected changes
 * ! The key in the workspace root node map should be the full path - and not just the displayname...
 * ! Refactor NewModel in Workspace/Editor handling!
 * ! TextBuffer should NOT have PathName, let TextBuffer Load/Save work take the PathName as an argument..
 * ! Workspace view must be able to react to changes in the model and also keep the current treeview status
 * ! When creating a new model we should ask the work-space for currently selected node!
 * ! deb package does not install resources to correct place (not sure where they end up)
 * ! Workspace, add meta-data to Workspace node's (Type, Filesize, etc..) - this can be a simple 'map' (YML map??)
 * ! Remove additional empty line on top when reading files
 * ! Handle read/write permissions
 * ! If typing is started and selection is active - we should delete the selection and replace with they typing..
 * ! starting editor with a new non-existing file doesn't create it
 * ! Workspace, refactor so that 'models' have specific nodes (currently they are indirect as the node has an array of models)
 * ! Workspace view should 'hide' folders starting with a dot (like: .git, .idea, .goatedit, etc..)
 * ! When opening a folder with an absolute path - check cwd is at the head of that path - otherwise change to it..
 * [!] Searching should be threaded, see if we can use some new CPP features for this - not needed(?) it is fast even though stupid
 * [!] Refactor initialization, split app init from data loading (data loading should happen afterwards - when UI is up and running)
 * ! Verify that Config can be merged with either 'Keep' or 'Override'
 * ! Implement loading strategy for asset loader (~/.config/gedit/.config/, ~/.gedit, Contents/MacOS/... / etc..)
 * ! Highlight of search results seems to be in screen coords (scrolling around when search results are active shows this)
 * ! Theme handling needed, currently the 'Config' class holds all theme related settings (which basically is just colors)
 * ! Workspace, pressing return/enter on selection should load (if needed) and set the model active in the editor
 * ! QuickCommandMode
 *   ! Allow cursor to be positioned at the 'C' input
 *   ! Ability to navigate through search results (next/previous) - should reposition the cursor
 *   ! Consider how to visualize various things coming from the quick-command mode
 *     ! you are in search and want 'hits', perhaps change 'C' to 'S' in the prefix??
 *   ! Require prefix '.' before entering commands (need states) as commands require bypassing short-cuts..
 *   ! When '.' is entered we should disable ASCII commands (like we do for search) otherwise certain key-combos are not valid!
 *     Illustrate: press './' and you will enter search... this should not be the case..
 * ! Switch to References everywhere, either C++ ref or shared_ptr type of refs (MyClass::Ref), there are still too many places using raw object pointers
 * ! Consolidate configuration (or sort it up properly) currently spread out between 'Editor', 'Config', 'RuntimeConfig'
 * ! CopyPaste, rewrite to use own ClipBoard, need to figure out how to handle text from OS clipboard...
 *   For SDL2 I use the SDL_CLIPBOARDUPDATE event to bring OS data in...  For NCurses I don't know yet..
 * ! ClipBoard needs the ability to call a function which can set the OS clipboard text (for SDL this is SDL_SetClipboardText)
 *   ex: ClipBoard::SetClipboardChangeDelegate([this](const std::vector<std::string> &data) { ... });
 * ! Remove the buffer manager class - not needed
 * ! Unsaved file should have '*' marking in the top..
 * ! Start work on history/undo
 * ! Cleanup/Refactor EditController/EditModel/TextBuffer - either remove one (controller) or have a clear distinction that changes must go through controller and other stuff (like search) through model
 * ! Action and Keymaps should store the modifier explicitly and not try to derive it from the masks in KeyMapping::ActionModifierFromMask
 * ! Something is causing sigsev, I strongly suspect that it is related to threading in Syntax Coloring / Language code
 *   [note: this was due to lines becoming invalid on certain operations while the parser was running]
 * ! Make large files > 10k lines read-only, alt. disable reparsing and syntax highlighting for large files..
 *   Disabling syntax can be deduced on-the fly by measuring the reparsing process..
 * ! Add change notification to line
 *   ! In TextBuffer, set change notification to all lines in the buffer
 *   ! In TextBuffer update the internal state when changed, clear on save..
 * ! Merge shell and 'Editor' bootstrap script capabilities and move to PluginExecutor [was: CommandController]
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
 * ! Figure out how to handle 'HasContentChanged' notifications to force redraws..
 *   a) be in the redraw loop and just do it (let the views take care of it)
 *   b) Somehow let a controller or view set a flag that a redraw is needed..
 * ! CommandController, NOTE: THIS IS QUITE THE TASK
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
//#include <SDL3/SDL_keyboard.h>
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
#include "Core/Views/TerminalView.h"
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


//extern char glbFillchar;      // this is a special case for NCurses when debugging drawing


#include <stdexcept>
#include <execinfo.h>
#include <exception>
#include <stdexcept>
#include <cstdlib>
#include <csignal>
#include <iostream>
#include "Core/backward.hpp"

// Testing out termination handling
// The following two blocks of code was taken from: https://stackoverflow.com/questions/61798770/questions-regarding-the-usage-of-set-terminate
//
// If this 'works' I should probably replace some of it with the new CPP features for stack-traces...
//

static void Unwind() {
    using namespace backward;

    StackTrace st; st.load_here(32);
    Printer p;
    p.object = true;
    p.color_mode = ColorMode::always;
    p.address = true;
    p.print(st, stderr);
}

// This function is used for handle segmental fault
inline void segfaultHandler(int signal __attribute__((unused)))
{
    std::cerr << "Segmentation fault (sig: " << signal << ")! backtrace: " << "\n";
    Unwind();
    abort();
}

// This is terminate handle function
inline void exceptionHandler()
{
    static bool triedThrow = false;
    try {
        if(!triedThrow) {
            triedThrow = true;
            throw;
        }
    }
    catch( const std::exception &e) {
        std::cerr << "Caught unhandled exception: " << e.what();
    }
    catch(...){}

    std::cerr << "backtrace:\n";
    Unwind();

//    void *stackArray[20];
//    size_t size = backtrace(stackArray, 10);
//    std::cerr << "Segmentation fault! backtrace: ";
//    char** backtrace = backtrace_symbols(stackArray, size);
//    for (size_t i = 0; i < size; i++) {
//        std::cerr << "\t" << backtrace[i];
//    }
    abort();
}



int main(int argc, const char **argv) {

    signal(SIGSEGV, segfaultHandler);
    std::set_terminate(exceptionHandler);


    Editor::Instance().Initialize(argc, argv);
    Editor::Instance().OpenScreen();


    // Note: This can be implicit
    RuntimeConfig::Instance().SetMainThreadID();

    //TestKeyBoardDriver();


    auto logger = gnilk::Logger::GetLogger("main");

    auto screen = RuntimeConfig::Instance().GetScreen();
    auto dimensions = screen->Dimensions();

    auto models = Editor::Instance().GetModels();
    auto workspace = Editor::Instance().GetWorkspace();
    for(auto m : models) {
        auto node = workspace->GetNodeFromModel(m);
        if (node == nullptr) {
            continue;
        }
        logger->Debug("File: %s",node->GetDisplayName().c_str());
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

    HSplitViewStatus hSplitViewStatus;
    rootView.AddView(&hSplitViewStatus);


    auto terminalView = TerminalView();
    //auto cmdView = CommandView();
    //hSplitViewStatus.SetLower(&cmdView);
    hSplitViewStatus.SetLower(&terminalView);

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

    rootView.AddTopView(&editorView, glbEditorView);
    rootView.AddTopView(&terminalView, glbTerminalView);
    rootView.AddTopView(&workspaceExplorer, glbWorkSpaceView);

//    WorkspaceView workspaceView;
//    ModalView dummy(dimensions, &workspaceView);
//    Runloop::ShowModal(&dummy);
//    exit(1);

    RuntimeConfig::Instance().SetRootView(&rootView);
    RuntimeConfig::Instance().SetQuickCmdView(&hSplitViewStatus);

    rootView.Initialize();
    rootView.InvalidateAll();

    Editor::Instance().RunPostInitalizationScript();

    // No model was given on startup - so let's focus in the ProjectView...
    if (Editor::Instance().GetActiveModel() == nullptr) {
        rootView.SetActiveTopViewByName(glbWorkSpaceView);
    }


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
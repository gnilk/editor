//
// Created by gnilk on 13.02.23.
//
/*
 * TO-DO List
 * 1)
 * ! New CompositionObject between View/Controller/Data => EditorModel
 *      ! Should hold an EditController, TextBuffer and ViewData
 *      ! Change the way EditView works, instead of owning the controller - the controller is set
 *      ! RuntimeConfiguration should have a function to retrieve the active EditorModel
 *      ! Move Cursor to EditorModel instance!!
 * - In BufferManager - make it possible to iterate through all buffers currently open..
 * - headerView -> Specialize SingleLineView to 'HeaderView' make the draw function
 * - new single view for status line / splitter -> make this a specific "HSplitView"
 *
 *
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

//
// Note: This is not needed for built in stuff (like navigation and so forth)
//       But once we want scriptable keyboard short-cuts we need a proxy...
//

static auto defaultDispatch = [](kAction action) {
    RuntimeConfig::Instance().RootView()->OnAction(action);
};

static std::unordered_map<kAction, ActionDispatch> actionHandlers {
        {kAction::kActionPageUp, ActionDispatch(defaultDispatch) },
        {kAction::kActionPageDown, ActionDispatch(defaultDispatch) },
};

static std::unordered_map<std::string, kAction> strToActionMap = {
        {"NavigateLineDown", kAction::kActionLineDown},
        {"NavigateLineUp", kAction::kActionLineUp},
        {"NavigatePageDown", kAction::kActionPageDown},
        {"NavigatePageUp", kAction::kActionPageUp},
        {"NavigateLineEnd", kAction::kActionLineEnd},
        {"NavigateLineHome", kAction::kActionLineHome},
        {"NavigateHome", kAction::kActionBufferStart},
        {"NavigateEnd", kAction::kActionBufferEnd},
        {"NavigateLineStepLeft", kAction::kActionLineStepSingleLeft},
        {"NavigateLineStepRight", kAction::kActionLineStepSingleRight},
        {"CommitLine", kAction::kActionCommitLine},
        {"GotoFirstLine", kAction::kActionGotoFirstLine},
        {"GotoLastLine", kAction::kActionGotoLastLine},
};


static std::unordered_map<std::string, Keyboard::kKeyCode> strToKeyCodeMap = {
        {"KeyCode_None",Keyboard::kKeyCode_None},
        {"KeyCode_Return",Keyboard::kKeyCode_Return},
        {"KeyCode_Escape",Keyboard::kKeyCode_Escape},
        {"KeyCode_Backspace",Keyboard::kKeyCode_Backspace},
        {"KeyCode_Tab",Keyboard::kKeyCode_Tab},
        {"KeyCode_Space",Keyboard::kKeyCode_Space},
        {"KeyCode_F1",Keyboard::kKeyCode_F1},
        {"KeyCode_F2",Keyboard::kKeyCode_F2},
        {"KeyCode_F3",Keyboard::kKeyCode_F3},
        {"KeyCode_F4",Keyboard::kKeyCode_F4},
        {"KeyCode_F5",Keyboard::kKeyCode_F5},
        {"KeyCode_F6",Keyboard::kKeyCode_F6},
        {"KeyCode_F7",Keyboard::kKeyCode_F7},
        {"KeyCode_F8",Keyboard::kKeyCode_F8},
        {"KeyCode_F9",Keyboard::kKeyCode_F9},
        {"KeyCode_F10",Keyboard::kKeyCode_F10},
        {"KeyCode_F11",Keyboard::kKeyCode_F11},
        {"KeyCode_F12",Keyboard::kKeyCode_F12},
        {"KeyCode_PrintScreen",Keyboard::kKeyCode_PrintScreen},
        {"KeyCode_ScrollLock",Keyboard::kKeyCode_ScrollLock},
        {"KeyCode_Pause",Keyboard::kKeyCode_Pause},
        {"KeyCode_Insert",Keyboard::kKeyCode_Insert},
        {"KeyCode_Home",Keyboard::kKeyCode_Home},
        {"KeyCode_PageUp",Keyboard::kKeyCode_PageUp},
        {"KeyCode_DeleteForward",Keyboard::kKeyCode_DeleteForward},
        {"KeyCode_End",Keyboard::kKeyCode_End},
        {"KeyCode_PageDown",Keyboard::kKeyCode_PageDown},
        {"KeyCode_LeftArrow",Keyboard::kKeyCode_LeftArrow},
        {"KeyCode_RightArrow",Keyboard::kKeyCode_RightArrow},
        {"KeyCode_DownArrow",Keyboard::kKeyCode_DownArrow},
        {"KeyCode_UpArrow",Keyboard::kKeyCode_UpArrow},
        {"KeyCode_NumLock",Keyboard::kKeyCode_NumLock},
};

static std::unordered_map<std::string, int> strToModifierMap = {
        {"Shift", Keyboard::kMod_LeftShift | Keyboard::kMod_RightShift},
        {"Ctrl", Keyboard::kMod_LeftCtrl | Keyboard::kMod_RightCtrl},
        {"Alt", Keyboard::kMod_LeftAlt | Keyboard::kMod_RightAlt},
        {"Cmd", Keyboard::kMod_LeftCommand | Keyboard::kMod_RightCommand},
        // Alias
        {"Control", Keyboard::kMod_LeftCtrl | Keyboard::kMod_RightCtrl},
        {"Alternate", Keyboard::kMod_LeftAlt | Keyboard::kMod_RightAlt},
        {"Command", Keyboard::kMod_LeftCommand | Keyboard::kMod_RightCommand},
        // Left/Right distinction
        {"LeftShift", Keyboard::kMod_LeftShift},
        {"RightShift", Keyboard::kMod_RightShift},
        {"LeftAlt", Keyboard::kMod_LeftAlt},
        {"RightAlt", Keyboard::kMod_RightAlt},
        {"LeftCmd", Keyboard::kMod_LeftCommand},
        {"RightCmd", Keyboard::kMod_RightCommand},
        // Alias
        {"LeftAlternate", Keyboard::kMod_LeftAlt},
        {"RightAlternate", Keyboard::kMod_RightAlt},
        {"LeftCommand", Keyboard::kMod_LeftCommand},
        {"RightCommand", Keyboard::kMod_RightCommand},
        // Using KeyCode prefix
        {"KeyCode_Shift", Keyboard::kMod_LeftShift | Keyboard::kMod_RightShift},
        {"KeyCode_Ctrl", Keyboard::kMod_LeftCtrl | Keyboard::kMod_RightCtrl},
        {"KeyCode_Alt", Keyboard::kMod_LeftAlt | Keyboard::kMod_RightAlt},
        {"KeyCode_Cmd", Keyboard::kMod_LeftCommand | Keyboard::kMod_RightCommand},
        // Alias
        {"KeyCode_Control", Keyboard::kMod_LeftCtrl | Keyboard::kMod_RightCtrl},
        {"KeyCode_Alternate", Keyboard::kMod_LeftAlt | Keyboard::kMod_RightAlt},
        {"KeyCode_Command", Keyboard::kMod_LeftCommand | Keyboard::kMod_RightCommand},
        // Left/Right distinction
        {"KeyCode_LeftShift", Keyboard::kMod_LeftShift},
        {"KeyCode_RightShift", Keyboard::kMod_RightShift},
        {"KeyCode_LeftAlt", Keyboard::kMod_LeftAlt},
        {"KeyCode_RightAlt", Keyboard::kMod_RightAlt},
        {"KeyCode_LeftCmd", Keyboard::kMod_LeftCommand},
        {"KeyCode_RightCmd", Keyboard::kMod_RightCommand},
        // Alias
        {"KeyCode_LeftAlternate", Keyboard::kMod_LeftAlt},
        {"KeyCode_RightAlternate", Keyboard::kMod_RightAlt},
        {"KeyCode_LeftCommand", Keyboard::kMod_LeftCommand},
        {"KeyCode_RightCommand", Keyboard::kMod_RightCommand},

};


static std::unordered_map<Keyboard::kKeyCode, std::string> keyCodeToStrMap = {
    {Keyboard::kKeyCode_None,"kKeyCode_None"},
    {Keyboard::kKeyCode_Return,"kKeyCode_Return"},
    {Keyboard::kKeyCode_Escape,"kKeyCode_Escape"},
    {Keyboard::kKeyCode_Backspace,"kKeyCode_Backspace"},
    {Keyboard::kKeyCode_Tab,"kKeyCode_Tab"},
    {Keyboard::kKeyCode_Space,"kKeyCode_Space"},
    {Keyboard::kKeyCode_F1,"kKeyCode_F1"},
    {Keyboard::kKeyCode_F2,"kKeyCode_F2"},
    {Keyboard::kKeyCode_F3,"kKeyCode_F3"},
    {Keyboard::kKeyCode_F4,"kKeyCode_F4"},
    {Keyboard::kKeyCode_F5,"kKeyCode_F5"},
    {Keyboard::kKeyCode_F6,"kKeyCode_F6"},
    {Keyboard::kKeyCode_F7,"kKeyCode_F7"},
    {Keyboard::kKeyCode_F8,"kKeyCode_F8"},
    {Keyboard::kKeyCode_F9,"kKeyCode_F9"},
    {Keyboard::kKeyCode_F10,"kKeyCode_F10"},
    {Keyboard::kKeyCode_F11,"kKeyCode_F11"},
    {Keyboard::kKeyCode_F12,"kKeyCode_F12"},
    {Keyboard::kKeyCode_PrintScreen,"kKeyCode_PrintScreen"},
    {Keyboard::kKeyCode_ScrollLock,"kKeyCode_ScrollLock"},
    {Keyboard::kKeyCode_Pause,"kKeyCode_Pause"},
    {Keyboard::kKeyCode_Insert,"kKeyCode_Insert"},
    {Keyboard::kKeyCode_Home,"kKeyCode_Home"},
    {Keyboard::kKeyCode_PageUp,"kKeyCode_PageUp"},
    {Keyboard::kKeyCode_DeleteForward,"kKeyCode_DeleteForward"},
    {Keyboard::kKeyCode_End,"kKeyCode_End"},
    {Keyboard::kKeyCode_PageDown,"kKeyCode_PageDown"},
    {Keyboard::kKeyCode_LeftArrow,"kKeyCode_LeftArrow"},
    {Keyboard::kKeyCode_RightArrow,"kKeyCode_RightArrow"},
    {Keyboard::kKeyCode_DownArrow,"kKeyCode_DownArrow"},
    {Keyboard::kKeyCode_UpArrow,"kKeyCode_UpArrow"},
    {Keyboard::kKeyCode_NumLock,"kKeyCode_NumLock"},
};

static std::vector<ActionItem::Ref> actionItems;

static void TestKeyMappings() {

    //
    // There are several different categories of actions
    // View - updates to navigation and similar (arrow keys, etc..)
    // Content - impact/updates to content (save, replace, etc..)
    // Workspace - All open files (compile, search in multiple files, etc..)
    //
    // There should be an API class for each category..
    // Note: There can probably be combined categories as well..
    //       Like: Search -> need content but updates view
    //
    // An API is a collection functions
    // The applicable classes (contex) need to implement the API baseclass..
    //
    // Setup lambdas for all action to provide proper routing to the system
    //

    // This is an example of routing..
    auto defaultActionNavDown = []() {
        auto rootView = static_cast<RootView *>(RuntimeConfig::Instance().RootView());
        rootView->TopView();    // Do something with it...
    };

    auto defaultActionOnContentChanged = [](void *sender) {
        auto model = RuntimeConfig::Instance().ActiveEditorModel();
        // diff and cache a copy of this in the background...
    };

    auto defaultAutoSave = []() {
        auto allModels = Editor::Instance().GetModels();
        for(auto &m : allModels) {
            // m->Save();
        }
    };




    if (!Config::Instance().HasKey("defaultkeymap")) {
        exit(1);
    }
    auto logger = gnilk::Logger::GetLogger("KeyMapping");
    auto keymap = Config::Instance().GetMap("defaultkeymap");
    std::string modifier;


    // Parse a keycode map...
    //   <keyCode> : <action>
    //
    // The keycode can be:
    //
    //  keycode                     A single key code, like: kKeycode_PageUp  (see: Keyboard::kKeyCode)
    //  <mod>+[<mod>]+<keycode>     An array of added modifiers + the keycode
    //
    // Example:
    //    KeyCode_Shift + KeyCode_UpArrow : NavigatePageUp,
    //    KeyCode_Ctrl + KeyCode_Shift + KeyCode_UpArrow : NavigateHome,
    //
    // Each combination maps to an action.
    //

    for (auto &kvp : keymap) {
        auto nameKeyCode = std::string(kvp.first);
        int modifierMask = 0;

        // Split the keycode in parts
        std::vector<std::string> keyCodeParts;
        strutil::split(keyCodeParts, nameKeyCode.c_str(), '+');

        // if '+' was not present, we only have 1 (size() == 1)
        if (keyCodeParts.size() == 0) {
            logger->Error("Fatal, keycode is horribly wrong...");
            exit(1);
        } else if (keyCodeParts.size() == 1) {
            // We have no mask - just a keycode
            nameKeyCode = keyCodeParts[0];
        } else {
            // We have more tokens - this indicates modifiers and keycode...
            // FIXME: Split this...
            int idxKeyCode = -1;
            std::vector<std::string> modifiers;

            // Find the keycode - we allow: Modifier + KeyCode + Modifier
            for(int i=0;i<keyCodeParts.size();i++) {
                if (strToKeyCodeMap.find(keyCodeParts[i]) != strToKeyCodeMap.end()) {
                    idxKeyCode = i;
                    break;
                } else {
                    modifiers.push_back(keyCodeParts[i]);
                }
            }
            // Bail if no keycode - this is configuration error
            if (idxKeyCode < 0) {
                logger->Error("Configuration error: No keycode found in: '%s'", nameKeyCode.c_str());
                exit(1);
            }

            nameKeyCode = keyCodeParts[idxKeyCode];
            // Compile the modifier mask...
            for(auto m : modifiers) {
                if (!strToModifierMap[m]) {
                    logger->Error("No modifier for '%s'", m.c_str());
                    exit(1);
                }
                modifierMask |= strToModifierMap[m];
            }

            // Wow - we finally have the key-combination
            logger->Debug("nameKeyCode = %s, modifierMask: 0x%x", nameKeyCode.c_str(), modifierMask);
        }

        // FIXME: This can be removed, covered by if (size==1) -> when splitting the above if-mess to functions
        if (!strToKeyCodeMap[nameKeyCode]) {
            logger->Error("Mapping missing for: %s\n", nameKeyCode.c_str());
            continue;
        }

        // Grab the actual keyCode (the modifiers are done already)
        // modifierMask + keyCode => full key combination for the action (see next...)
        auto keyCode = strToKeyCodeMap[nameKeyCode];

        // Looping over the actions in the config file...
        auto strAction = kvp.second;

        if (strToActionMap.find(strAction) == strToActionMap.end()) {
            logger->Error("No action for: %s:%s", nameKeyCode.c_str(), strAction.c_str());
            continue;
        }
        logger->Debug("Action %s, keyCode=%s, modifierMask=0x%x", strAction.c_str(), nameKeyCode.c_str(), modifierMask);
        // Compose an action based on modifierMask + keyCode + kAction
        auto actionItem = ActionItem::Create(strToActionMap[strAction], modifierMask, keyCode, strAction);
        actionItems.push_back(actionItem);
    }
}

kAction TestActionHandleKeyPress(const KeyPress &keyPress) {
    auto logger = gnilk::Logger::GetLogger("KeyMapping");
    for(auto &actionItem : actionItems) {
        if (actionItem->MatchKeyPress(keyPress)) {
            logger->Debug("ActionItem found!!!");
            return actionItem->GetAction();
        }
    }
    return kAction::kActionNone;
}


static void SetupLogger() {
}
extern char glbFillchar;

int main(int argc, const char **argv) {

    Editor::Instance().Initialize(argc, argv);

    TestKeyMappings();

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
        bool redraw = false;
        glbFillchar = 'a';

        auto keyPress = keyboardDriver->GetKeyPress();
        if (keyPress.IsAnyValid()) {

            logger->Debug("KeyPress Valid - passing on...");
            auto action = TestActionHandleKeyPress(keyPress);
            if (action != gedit::kAction::kActionNone) {
                logger->Debug("Action found, handling as such!");
                rootView.TopView()->OnAction(action);
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
        }
    }
    logger->Debug("Left main loop, closing graphics subsystem");
    screen->Close();
    return 0;

    return -1;
}
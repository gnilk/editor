//
// Created by gnilk on 24.03.23.
//

//
// Contains translation tables between keycodes, actions and strings...
// A view can react on the actions through 'OnAction'
//

#include "KeyMapping.h"
#include "Action.h"
#include "Core/Config/Config.h"
#include "Core/RuntimeConfig.h"
#include "KeyCodes.h"
#include "logger.h"
#include <cctype>
#include <unordered_map>
#include <string>

using namespace gedit;

// mapping tables...
static std::unordered_map<std::string, kModifier> strToModifierMap = {
        {"SelectionModifier", kModifier::kModifierSelection},
        {"Selection", kModifier::kModifierSelection},

        {"CopyPasteModifier", kModifier::kModifierCopyPaste},
        {"CopyPaste", kModifier::kModifierCopyPaste},

        {"UINavigationModifier", kModifier::kModifierUINavigation},
        {"UINavigation", kModifier::kModifierUINavigation},
};
//
// TO-DO consider renaming the actions
//
static std::unordered_map<std::string, kAction> strToActionMap = {
        {"NavigateLineDown",      kAction::kActionLineDown},
        {"NavigateLineUp",        kAction::kActionLineUp},
        {"NavigatePageDown",      kAction::kActionPageDown},
        {"NavigatePageUp",        kAction::kActionPageUp},
        {"NavigateLineEnd",       kAction::kActionLineEnd},
        {"NavigateLineHome",      kAction::kActionLineHome},
        {"NavigateHome",          kAction::kActionBufferStart},
        {"NavigateEnd",           kAction::kActionBufferEnd},
        {"NavigateLeft",          kAction::kActionLineLeft},
        {"NavigateRight",         kAction::kActionLineRight},
        {"NavigateWordLeft",      kAction::kActionLineWordLeft},
        {"NavigateWordRight",     kAction::kActionLineWordRight},
        {"CommitLine",            kAction::kActionCommitLine},
        {"GotoFirstLine",         kAction::kActionGotoFirstLine},
        {"GotoLastLine",          kAction::kActionGotoLastLine},
        {"GotoBottomLine",        kAction::kActionGotoBottomLine},
        {"GotoTopLine",           kAction::kActionGotoTopLine},
        {"CycleActiveView",       kAction::kActionCycleActiveView},
        {"CycleActiveViewNext",       kAction::kActionCycleActiveViewNext},
        {"CycleActiveViewPrev",       kAction::kActionCycleActiveViewPrev},
        {"CycleActiveEditor",     kAction::kActionCycleActiveEditor},
        {"EditBackspace",         kAction::kActionEditBackspace},
        {"CopyToClipboard",       kAction::kActionCopyToClipboard},
        {"PasteFromClipboard",    kAction::kActionPasteFromClipboard},
        {"CloseModal",            kAction::kActionCloseModal},
        {"CommentLine",           kAction::kActionInsertLineComment}
};


static std::unordered_map<std::string, Keyboard::kKeyCode> strToKeyCodeMap = {
        {"KeyCode_None",          Keyboard::kKeyCode_None},
        {"KeyCode_Return",        Keyboard::kKeyCode_Return},
        {"KeyCode_Escape",        Keyboard::kKeyCode_Escape},
        {"KeyCode_Backspace",     Keyboard::kKeyCode_Backspace},
        {"KeyCode_Tab",           Keyboard::kKeyCode_Tab},
        {"KeyCode_Space",         Keyboard::kKeyCode_Space},
        {"KeyCode_F1",            Keyboard::kKeyCode_F1},
        {"KeyCode_F2",            Keyboard::kKeyCode_F2},
        {"KeyCode_F3",            Keyboard::kKeyCode_F3},
        {"KeyCode_F4",            Keyboard::kKeyCode_F4},
        {"KeyCode_F5",            Keyboard::kKeyCode_F5},
        {"KeyCode_F6",            Keyboard::kKeyCode_F6},
        {"KeyCode_F7",            Keyboard::kKeyCode_F7},
        {"KeyCode_F8",            Keyboard::kKeyCode_F8},
        {"KeyCode_F9",            Keyboard::kKeyCode_F9},
        {"KeyCode_F10",           Keyboard::kKeyCode_F10},
        {"KeyCode_F11",           Keyboard::kKeyCode_F11},
        {"KeyCode_F12",           Keyboard::kKeyCode_F12},
        {"KeyCode_PrintScreen",   Keyboard::kKeyCode_PrintScreen},
        {"KeyCode_ScrollLock",    Keyboard::kKeyCode_ScrollLock},
        {"KeyCode_Pause",         Keyboard::kKeyCode_Pause},
        {"KeyCode_Insert",        Keyboard::kKeyCode_Insert},
        {"KeyCode_Home",          Keyboard::kKeyCode_Home},
        {"KeyCode_PageUp",        Keyboard::kKeyCode_PageUp},
        {"KeyCode_DeleteForward", Keyboard::kKeyCode_DeleteForward},
        {"KeyCode_End",           Keyboard::kKeyCode_End},
        {"KeyCode_PageDown",      Keyboard::kKeyCode_PageDown},
        {"KeyCode_LeftArrow",     Keyboard::kKeyCode_LeftArrow},
        {"KeyCode_RightArrow",    Keyboard::kKeyCode_RightArrow},
        {"KeyCode_DownArrow",     Keyboard::kKeyCode_DownArrow},
        {"KeyCode_UpArrow",       Keyboard::kKeyCode_UpArrow},
        {"KeyCode_NumLock",       Keyboard::kKeyCode_NumLock},
};

static std::unordered_map<std::string, int> strToModifierBitMaskMap = {
        {"Shift",                  Keyboard::kMod_LeftShift | Keyboard::kMod_RightShift},
        {"Ctrl",                   Keyboard::kMod_LeftCtrl | Keyboard::kMod_RightCtrl},
        {"Alt",                    Keyboard::kMod_LeftAlt | Keyboard::kMod_RightAlt},
        {"Cmd",                    Keyboard::kMod_LeftCommand | Keyboard::kMod_RightCommand},
        // Alias
        {"Control",                Keyboard::kMod_LeftCtrl | Keyboard::kMod_RightCtrl},
        {"Alternate",              Keyboard::kMod_LeftAlt | Keyboard::kMod_RightAlt},
        {"Command",                Keyboard::kMod_LeftCommand | Keyboard::kMod_RightCommand},
        // Left/Right distinction
        {"LeftShift",              Keyboard::kMod_LeftShift},
        {"RightShift",             Keyboard::kMod_RightShift},
        {"LeftAlt",                Keyboard::kMod_LeftAlt},
        {"RightAlt",               Keyboard::kMod_RightAlt},
        {"LeftCmd",                Keyboard::kMod_LeftCommand},
        {"RightCmd",               Keyboard::kMod_RightCommand},
        // Alias
        {"LeftAlternate",          Keyboard::kMod_LeftAlt},
        {"RightAlternate",         Keyboard::kMod_RightAlt},
        {"LeftCommand",            Keyboard::kMod_LeftCommand},
        {"RightCommand",           Keyboard::kMod_RightCommand},
        // Using KeyCode prefix
        {"KeyCode_Shift",          Keyboard::kMod_LeftShift | Keyboard::kMod_RightShift},
        {"KeyCode_Ctrl",           Keyboard::kMod_LeftCtrl | Keyboard::kMod_RightCtrl},
        {"KeyCode_Alt",            Keyboard::kMod_LeftAlt | Keyboard::kMod_RightAlt},
        {"KeyCode_Cmd",            Keyboard::kMod_LeftCommand | Keyboard::kMod_RightCommand},
        // Alias
        {"KeyCode_Control",        Keyboard::kMod_LeftCtrl | Keyboard::kMod_RightCtrl},
        {"KeyCode_Alternate",      Keyboard::kMod_LeftAlt | Keyboard::kMod_RightAlt},
        {"KeyCode_Command",        Keyboard::kMod_LeftCommand | Keyboard::kMod_RightCommand},
        // Left/Right distinction
        {"KeyCode_LeftShift",      Keyboard::kMod_LeftShift},
        {"KeyCode_RightShift",     Keyboard::kMod_RightShift},
        {"KeyCode_LeftAlt",        Keyboard::kMod_LeftAlt},
        {"KeyCode_RightAlt",       Keyboard::kMod_RightAlt},
        {"KeyCode_LeftCmd",        Keyboard::kMod_LeftCommand},
        {"KeyCode_RightCmd",       Keyboard::kMod_RightCommand},
        // Alias
        {"KeyCode_LeftAlternate",  Keyboard::kMod_LeftAlt},
        {"KeyCode_RightAlternate", Keyboard::kMod_RightAlt},
        {"KeyCode_LeftCommand",    Keyboard::kMod_LeftCommand},
        {"KeyCode_RightCommand",   Keyboard::kMod_RightCommand},

};


static std::unordered_map<Keyboard::kKeyCode, std::string> keyCodeToStrMap = {
        {Keyboard::kKeyCode_None,          "kKeyCode_None"},
        {Keyboard::kKeyCode_Return,        "kKeyCode_Return"},
        {Keyboard::kKeyCode_Escape,        "kKeyCode_Escape"},
        {Keyboard::kKeyCode_Backspace,     "kKeyCode_Backspace"},
        {Keyboard::kKeyCode_Tab,           "kKeyCode_Tab"},
        {Keyboard::kKeyCode_Space,         "kKeyCode_Space"},
        {Keyboard::kKeyCode_F1,            "kKeyCode_F1"},
        {Keyboard::kKeyCode_F2,            "kKeyCode_F2"},
        {Keyboard::kKeyCode_F3,            "kKeyCode_F3"},
        {Keyboard::kKeyCode_F4,            "kKeyCode_F4"},
        {Keyboard::kKeyCode_F5,            "kKeyCode_F5"},
        {Keyboard::kKeyCode_F6,            "kKeyCode_F6"},
        {Keyboard::kKeyCode_F7,            "kKeyCode_F7"},
        {Keyboard::kKeyCode_F8,            "kKeyCode_F8"},
        {Keyboard::kKeyCode_F9,            "kKeyCode_F9"},
        {Keyboard::kKeyCode_F10,           "kKeyCode_F10"},
        {Keyboard::kKeyCode_F11,           "kKeyCode_F11"},
        {Keyboard::kKeyCode_F12,           "kKeyCode_F12"},
        {Keyboard::kKeyCode_PrintScreen,   "kKeyCode_PrintScreen"},
        {Keyboard::kKeyCode_ScrollLock,    "kKeyCode_ScrollLock"},
        {Keyboard::kKeyCode_Pause,         "kKeyCode_Pause"},
        {Keyboard::kKeyCode_Insert,        "kKeyCode_Insert"},
        {Keyboard::kKeyCode_Home,          "kKeyCode_Home"},
        {Keyboard::kKeyCode_PageUp,        "kKeyCode_PageUp"},
        {Keyboard::kKeyCode_DeleteForward, "kKeyCode_DeleteForward"},
        {Keyboard::kKeyCode_End,           "kKeyCode_End"},
        {Keyboard::kKeyCode_PageDown,      "kKeyCode_PageDown"},
        {Keyboard::kKeyCode_LeftArrow,     "kKeyCode_LeftArrow"},
        {Keyboard::kKeyCode_RightArrow,    "kKeyCode_RightArrow"},
        {Keyboard::kKeyCode_DownArrow,     "kKeyCode_DownArrow"},
        {Keyboard::kKeyCode_UpArrow,       "kKeyCode_UpArrow"},
        {Keyboard::kKeyCode_NumLock,       "kKeyCode_NumLock"},
};

KeyMapping &KeyMapping::Instance() {
    static KeyMapping glbKeyMapping;
    glbKeyMapping.Initialize();
    return glbKeyMapping;
}

//
// This will build the action maps..
//
bool KeyMapping::Initialize() {
    if (isInitialized) {
        return true;
    }

    if (!RebuildActionMapping()) {
        return false;
    }

//    if (!RebuildActionMapping()) {
//        return false;
//    }
    // Do stuff
    isInitialized = true;
    return true;
}
const std::string &KeyMapping::ModifierName(kModifier modifier) {
    static std::string noname = "<none>";   // this is just for debugging purposes anyway...
    for(auto &[nameModifier, modf] : strToModifierMap) {
        if (modf == modifier) {
            return nameModifier;
        }
    }
    return noname;
}


const std::string &KeyMapping::KeyCodeName(const Keyboard::kKeyCode keyCode) {
    return keyCodeToStrMap[keyCode];
}

// Note: This is a slow operation, it is used for debugging purposes...
const std::string &KeyMapping::ActionName(const kAction action) {
    static std::string empty = {};
    for (auto &it: strToActionMap) {
        if (it.second == action) {
            return it.first;
        }
    }
    return empty;
}

int KeyMapping::ModifierMaskFromString(const std::string &strModifiers) {
    return strToModifierBitMaskMap[strModifiers];
}

kAction KeyMapping::ActionFromName(const std::string &strAction) {
    return strToActionMap[strAction];
}


// Translate from KeyPress to Action..
std::optional<KeyPressAction> KeyMapping::ActionFromKeyPress(const KeyPress &keyPress) {
    auto logger = gnilk::Logger::GetLogger("KeyMapping");
    for (auto &actionItem: actionItems) {
        if (actionItem->MatchKeyPress(keyPress)) {
            logger->Debug("ActionItem found!!!");
            KeyPressAction kpAction;
            kpAction.action = actionItem->GetAction();
            kpAction.modifierMask = keyPress.modifiers; // redundant..
            kpAction.modifier = ModifierFromMask(keyPress.modifiers);
            kpAction.keyPress = keyPress;
            return kpAction;
        }
    }
    return {};
}

static bool IsAsciiKeyCode(const std::string &strKeyCode) {
    if (strKeyCode.size() != 3) return false;
    if (strKeyCode[0] != '\'') return false;
    if (strKeyCode[2] != '\'') return false;
    // FIXME: Better check??
    if (std::isspace(strKeyCode[1])) return false;
    return true;
}


//
// Go through configuration and rebuild the action map
//
bool KeyMapping::RebuildActionMappingOld() {

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
//    auto defaultActionNavDown = []() {
//        auto rootView = static_cast<RootView *>(RuntimeConfig::Instance().RootView());
//        rootView->TopView();    // Do something with it...
//    };
//
//    auto defaultActionOnContentChanged = [](void *sender) {
//        auto model = RuntimeConfig::Instance().ActiveEditorModel();
//        // diff and cache a copy of this in the background...
//    };
//
//    auto defaultAutoSave = []() {
//        auto allModels = Editor::Instance().GetModels();
//        for(auto &m : allModels) {
//            // m->Save();
//        }
//    };
//



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

    // Clear current action map...
    actionItems.clear();

    for (auto &kvp : keymap) {
        auto nameKeyCode = std::string(kvp.first);  // This one is mutable..
        auto origKeyPressCombo = std::string(kvp.first);
        int modifierMask = 0;

        logger->Debug("Parsing: '%s'", nameKeyCode.c_str());
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
            int idxKeyCode = -1;
            std::vector<std::string> modifiers;

            // Find the keycode - we allow: Modifier + KeyCode + Modifier
            for(int i=0;i<keyCodeParts.size();i++) {

                if (IsAsciiKeyCode(keyCodeParts[i]) || (strToKeyCodeMap.find(keyCodeParts[i]) != strToKeyCodeMap.end())) {
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
                if (!strToModifierBitMaskMap[m]) {
                    logger->Error("No modifier for '%s'", m.c_str());
                    exit(1);
                }
                modifierMask |= strToModifierBitMaskMap[m];
            }

            // Wow - we finally have the key-combination
            logger->Debug("  nameKeyCode = '%s', modifierMask: 0x%x", nameKeyCode.c_str(), modifierMask);
        }

        // Looping over the actions in the config file...
        auto strAction = kvp.second;

        if (strToActionMap.find(strAction) == strToActionMap.end()) {
            logger->Error("No action named '%s' for: %s:%s", strAction.c_str(), nameKeyCode.c_str(), strAction.c_str());
            exit(1);
            continue;
        }


        if (IsAsciiKeyCode(nameKeyCode)) {
            auto asciiKeyCode = nameKeyCode[1];
            logger->Debug("  Action '%s', from '%s' (ascii code=%s, modifierMask=0x%x)", strAction.c_str(), origKeyPressCombo.c_str(), nameKeyCode.c_str(), modifierMask);
            auto actionItem =ActionItem::Create(strToActionMap[strAction], modifierMask, asciiKeyCode, strAction);
            actionItems.push_back(actionItem);


        } else {
            // FIXME: Support 'f' with modifierMask.. (for CTRL+'f' etc...)
            if (!strToKeyCodeMap[nameKeyCode]) {
                logger->Error("Mapping missing for: %s\n", nameKeyCode.c_str());
                continue;
            }

            // Grab the actual keyCode (the modifiers are done already)
            // modifierMask + keyCode => full key combination for the action (see next...)
            auto keyCode = strToKeyCodeMap[nameKeyCode];

            logger->Debug("  Action '%s', from '%s' (keyCode=%s, modifierMask=0x%x)", strAction.c_str(), origKeyPressCombo.c_str(), nameKeyCode.c_str(), modifierMask);
            // Compose an action based on modifierMask + keyCode + kAction
            auto actionItem = ActionItem::Create(strToActionMap[strAction], modifierMask, keyCode, strAction);
            actionItems.push_back(actionItem);
        }

    }

    return true;
}


// Temporary during building of the keymap actions..
struct TempKeyMap {
    bool isOptional = false;
    bool isModifier = false;
    std::string keyCodeName;
    uint8_t permutationMask = 0;
    uint32_t modifiermask = 0;
};
//
// New parser, let this sit next to the old one
//
bool KeyMapping::RebuildActionMapping() {
    if (!Config::Instance().HasKey("keymap")) {
        exit(1);
    }

    auto logger = gnilk::Logger::GetLogger("KeyMapping");
    auto keymap = Config::Instance()["keymap"];

    // Verify we have 'actions' (this is the most important)
    if (!keymap.HasKey("actions")) {
        logger->Error("Keymap must at least have an action section!");
        return false;

    }

    // Build up the modifier table
    if (keymap.HasKey("modifiers")) {
        auto keymapModifiers = keymap.GetMap("modifiers");
        if (!ParseModifiers(keymapModifiers)) {
            return false;
        }
    }

    auto keymapModifiers = keymap.GetMap("modifiers");
    auto keymapActions = keymap.GetMap("actions");

    /*
     * Rework the parser
     * We have two sections in the keymap; aliases and actions
     * Modifers are defined first like:
     *  mymodifiers = keycode_name
     *
     *  Aliases allow for changing the whole keymap for certain keys, like the CopyPaste modifier is 'CMD' on macOS and 'CTRL' on Windows
     *
     * The action part looks like:
     *  action = list of modifier + key
     *
     * By adding a '@' infront of a key or modifier you make that an optional modifier:
     *
     * Like:
     * NavigateDown = KeyCode_DownArrow + @SelectionModifier
     *
     * This will (in essence) create two entries in the final action mapping
     * 1) NavigateDown = KeyCode_DownArrow
     * 2) NavigateDown = KeyCode_DownArrow + KeyCode_Shift
     *
     * (Assuming shift = 'SelectionModifier')
     *
     * The editor will then see 'SelectionModifier = KeyCode_Shift' and enable text-selection when that modifier is pressed..
     * Ergo, it is not needed to define multiple actions for each navigational action to support text-selection navigation...
     *
     */

    for (const auto &[actionName, keyPressCombo] : keymapActions) {
        logger->Debug("Parsing: '%s' = '%s'", actionName.c_str(), keyPressCombo.c_str());
        if (strToActionMap.find(actionName) == strToActionMap.end()) {
            logger->Error("Invalid Action '%s'; not found");
            return false;
        }
        auto action = strToActionMap.at(actionName);

        if (!ParseKeyPressCombinationString(action, keyPressCombo, keymapModifiers)) {
            logger->Error("KeyMap parse error for '%s : %s'", actionName.c_str(), keyPressCombo.c_str());
            return false;
        }
    }
    logger->Debug("**** PARSE OK ****");
    return true;
}

//
// Parse and build the modifier table
//
bool KeyMapping::ParseModifiers(const std::map<std::string, std::string> &keymapModifiers) {
    auto logger = gnilk::Logger::GetLogger("KeyMapping");
    logger->Debug("modifer section found");
    for(auto &[name, keyCode] : keymapModifiers) {
        if (strToModifierMap.find(name) == strToModifierMap.end()) {
            logger->Error("No modifier named '%s'", name.c_str());
            return false;
        }
        if (strToModifierBitMaskMap.find(keyCode) == strToModifierBitMaskMap.end()) {
            logger->Error("No keycode named '%s' for modifier '%s'", keyCode.c_str(), name.c_str());
            return false;
        }
        logger->Debug("  '%s' = '%s'", name.c_str(), keyCode.c_str());
        auto modifier = strToModifierMap.at(name);
        modifiers[modifier] = strToModifierBitMaskMap[keyCode];
    }
    return true;
}


//
// This parse an key press combination string on the form
// KeyCode_UpArray + <modifier> + @<modifier>
// the '@' sign is an optional modifier, which means we need to spit out two action per optional
// 1) the modifier mask is present
// 2) the modifier mask isn't present
//
// Thus, if you have multiple modifiers we need to permutate all possible options (which is 2^numOptionals) for the
// whole string. Therefore we first parse the list to a temporary structure, then we permutate and create the actions..
//
bool KeyMapping::ParseKeyPressCombinationString(kAction action, const std::string &keyPressCombo, const std::map<std::string, std::string> &keymapModifiers) {
    auto logger = gnilk::Logger::GetLogger("KeyMapping");

    bool isKeyCodeASCII = false;
    int  asciiKeyCode = 0;

    // Split the key combination string to a list of items
    std::vector<std::string> keypressList;
    strutil::split(keypressList, keyPressCombo.c_str(),'+');

    // The primary key-code, which shouldn't be a modifier!!!
    Keyboard::kKeyCode primaryKeycode = {};
    // The list of permutable modifiers..
    std::vector<TempKeyMap> keycodePermutationList = {};

    // How many optional's did we have - when permutating the optionals
    // we loop 2^numOptional
    int numOptional = 0;

    // loop over all
    for(auto &s : keypressList) {
        TempKeyMap tempKeyMap = {};
        std::string keycodeName;
        // Is this an optional???
        if (s.front() == '@') {
            tempKeyMap.isOptional = true;
            // optional creates a set of binary permutations between all optionals (shouldn't be too many)
            // in order to create all permutations we loop 2^numOptional and multiply the contributing bitmask
            // with either 1 or 0 before or:in..  Thus the first optional listens to bit 0 second to bit 1, and so forth...
            // Saving this here makes the permutation loop simpler...
            tempKeyMap.permutationMask = 1 << numOptional;
            numOptional++;
            // Strip away the '@'
            keycodeName = s.substr(1);
        } else {
            // play keycode...
            keycodeName = s;
        }

        // Substitue with a modifier (if any)
        if (keymapModifiers.find(keycodeName) != keymapModifiers.end()) {
            logger->Debug("  Modifier (%s) => %s", keycodeName.c_str(), keymapModifiers.at(keycodeName).c_str());
            keycodeName = keymapModifiers.at(keycodeName);
        }

        // check if the keycode is a modifier
        if (strToModifierBitMaskMap.find(keycodeName) != strToModifierBitMaskMap.end()) {
            tempKeyMap.isModifier = true;
            tempKeyMap.modifiermask = strToModifierBitMaskMap[keycodeName];
        } else if (strToKeyCodeMap.find(keycodeName) != strToKeyCodeMap.end()) {
            primaryKeycode = strToKeyCodeMap[keycodeName];
            // Not sure...
            if (tempKeyMap.isOptional) {
                logger->Error("KeyCode should not be optional - only modifiers - re: %s!", keycodeName.c_str());
                return false;
            }
        } else if (IsAsciiKeyCode(keycodeName)) {
            logger->Debug("Key is ASCII");
            if (tempKeyMap.isOptional) {
                logger->Error("ASCII Key should not be optional - only modifiers - re: %s!", keycodeName.c_str());
                return false;
            }
            // Note: we need to treat the 'ascii' key-codes differently
            isKeyCodeASCII = true;
            asciiKeyCode = keycodeName[1];
        } else {
            logger->Error("KeyCode '%s' not found",keycodeName.c_str());
            return false;
        }

        logger->Debug("  Store: %s [%s, %s]", keycodeName.c_str(),
                      tempKeyMap.isModifier?"modifier":"regular",
                      tempKeyMap.isOptional?"optional":"required");

        // If this is a modifier save it to the permutation list
        if (tempKeyMap.isModifier) {
            tempKeyMap.modifiermask = strToModifierBitMaskMap[keycodeName];
            keycodePermutationList.push_back(tempKeyMap);
        }
    }
    // Dump:
    logger->Debug("  KeyCode: %s", keyCodeToStrMap[primaryKeycode].c_str());

    for(auto &tempKeyMap : keycodePermutationList) {
        logger->Debug("    0x%.2x : [%s, %s]", tempKeyMap.modifiermask,
                      tempKeyMap.isModifier?"modifier":"regular",
                      tempKeyMap.isOptional?"optional":"required");

    }



    // permutations are 2^numOptional - it's binary...
    for(int i=0;i<pow(2, numOptional);i++) {
        int modifierMask = 0;
        for(auto &tempKeyMap : keycodePermutationList) {
            if (!tempKeyMap.isOptional) {
                modifierMask |= tempKeyMap.modifiermask;
            } else {
                // Magic goes here...
                // Each optional is assigned an on/off bit (permutationMask) if set we multiply the mask with '1' (i.e. the mask is added)
                // otherwise 0, the mask is not added (something | 0 = something)
                modifierMask |= (tempKeyMap.modifiermask * ((i & tempKeyMap.permutationMask)?1:0));
            }
        }
        logger->Debug("  %d : %s + 0x%.2x", i, keyCodeToStrMap[primaryKeycode].c_str(), modifierMask);

        ActionItem::Ref actionItem = {};
        if (isKeyCodeASCII) {
            actionItem =ActionItem::Create(action, modifierMask, asciiKeyCode, ActionName(action));
        } else {
            actionItem = ActionItem::Create(action, modifierMask, primaryKeycode, ActionName(action));
        }
        actionItems.push_back(actionItem);
    }

    return true;
}
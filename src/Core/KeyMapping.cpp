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
#include "Keyboard.h"
#include "logger.h"
#include <cctype>
#include <unordered_map>
#include <string>

using namespace gedit;

// mapping tables...
static std::unordered_map<std::string, kActionModifier> strToModifierMap = {
        {"SelectionModifier",    kActionModifier::kActionModifierSelection},
        {"Selection",            kActionModifier::kActionModifierSelection},

        {"CopyPasteModifier",    kActionModifier::kActionModifierCopyPaste},
        {"CopyPaste",            kActionModifier::kActionModifierCopyPaste},

        {"UINavigationModifier", kActionModifier::kActionModifierUINavigation},
        {"UINavigation",         kActionModifier::kActionModifierUINavigation},
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
        {"GotoLine",              kAction::kActionGotoLine},
        {"CycleActiveView",       kAction::kActionCycleActiveView},
        {"CycleActiveViewNext",       kAction::kActionCycleActiveViewNext},
        {"CycleActiveViewPrev",       kAction::kActionCycleActiveViewPrev},
        {"CycleActiveEditor",     kAction::kActionCycleActiveEditor},
        {"CycleBufferNext",      kAction::kActionCycleActiveBufferNext},
        {"CycleBufferPrev",      kAction::kActionCycleActiveBufferPrev},
        {"EditBackspace",         kAction::kActionEditBackspace},
        {"CopyToClipboard",       kAction::kActionCopyToClipboard},
        {"CutToClipboard",        kAction::kActionCutToClipboard},
        {"PasteFromClipboard",    kAction::kActionPasteFromClipboard},
        {"CloseModal",            kAction::kActionCloseModal},
        {"CommentLine",           kAction::kActionInsertLineComment},
        {"EnterCommandMode",      kAction::kActionEnterCommandMode},
        {"LeaveCommandMode",      kAction::kActionLeaveCommandMode},
        {"NextSearchResult",      kAction::kActionNextSearchResult},
        {"PrevSearchResult",      kAction::kActionPrevSearchResult},
        {"StartSearch",           kAction::kActionStartSearch},
        {"LastSearch",              kAction::kActionLastSearch},
        {"Undo",                    kAction::kActionUndo},
        {"Indent",                  kAction::kActionIndent},
        {"Unindent",                kAction::kActionUnindent},
        {"UIIncreaseViewWidth",     kAction::kActionIncreaseViewWidth},
        {"UIDecreaseViewWidth",     kAction::kActionDecreaseViewWidth},
        {"UIIncreaseViewHeight",    kAction::kActionIncreaseViewHeight},
        {"UIDecreaseViewHeight",    kAction::kActionDecreaseViewHeight},
        {"UIMaximizeViewHeight",    kAction::kActionMaximizeViewHeight},
        {"UISwitchToTerminal",      kAction::kActionSwitchToTerminal},
        {"UISwitchToEditor",        kAction::kActionSwitchToEditor},
        {"UISwitchToProject",       kAction::kActionSwitchToProject}
};






//KeyMapping &KeyMapping::Instance() {
//    static KeyMapping glbKeyMapping;
//    glbKeyMapping.Initialize();
//    return glbKeyMapping;
//}

KeyMapping::Ref KeyMapping::Create(const std::string &cfgNodeName) {
    auto instance = std::make_shared<KeyMapping>();
    if (!instance->Initialize(cfgNodeName)) {
        return nullptr;
    }
    return instance;
}

KeyMapping::Ref KeyMapping::Create(const ConfigNode &cfgNode) {
    auto instance = std::make_shared<KeyMapping>();
    if (!instance->Initialize(cfgNode)) {
        return nullptr;
    }
    return instance;

}


//
// This will build the action maps..
//
bool KeyMapping::Initialize(const std::string &cfgNodeName) {
    if (isInitialized) {
        return true;
    }

    if (!RebuildActionMapping(cfgNodeName)) {
        return false;
    }

    // Do stuff
    isInitialized = true;
    return true;
}

bool KeyMapping::Initialize(const ConfigNode &cfgNode) {
    if (isInitialized) {
        return true;
    }

    if (!RebuildActionMapping(cfgNode)) {
        return false;
    }

    // Do stuff
    isInitialized = true;
    return true;
}


const std::string &KeyMapping::ModifierName(kActionModifier modifier) {
    static std::string noname = "<none>";   // this is just for debugging purposes anyway...
    for(auto &[nameModifier, modf] : strToModifierMap) {
        if (modf == modifier) {
            return nameModifier;
        }
    }
    return noname;
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
            kpAction.actionModifier = actionItem->GetActionModifier();
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


// Temporary during building of the keymap actions..
struct TempKeyMap {
    bool isOptional = false;
    bool isModifier = false;
    std::string keyCodeName;
    uint8_t permutationMask = 0;
    uint32_t modifiermask = 0;
    std::optional<kActionModifier> actionModifier = {};
};

bool KeyMapping::RebuildActionMapping(const std::string &cfgNodeName) {
    if (!Config::Instance().HasKey(cfgNodeName)) {
        exit(1);
    }
    auto keymap = Config::Instance()[cfgNodeName];
    return RebuildActionMapping(keymap);
}

//
// New parser, let this sit next to the old one
//
bool KeyMapping::RebuildActionMapping(const ConfigNode &keymap) {

    auto logger = gnilk::Logger::GetLogger("KeyMapping");

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


    auto keymapModifiers = keymap.GetMap("modifiers");

    // We parse the RAW YAML Nodes here as we want to support mixed type of values..
    // An action can be an array of values and a single value.
    // IF we allow our configuration to support this - we can update this..
    // NOTE: I am using 'auto' deliberately in order to explicitly depend on YAML-CPP
    auto mixedNode = keymap.GetNode("actions");
    auto rawNode = mixedNode.GetDataNode();     // Fetch the raw YAML node
    for(auto it = rawNode.begin(); it != rawNode.end(); ++it) {
        auto key = it->first;       // RAW YAML nodes
        auto value = it->second;    // RAW YAML nodes
        if (!key.IsScalar()) {
            logger->Error("action must be string/scalar!");
            return false;
        }
        auto actionName = key.Scalar();
        if (strToActionMap.find(actionName) == strToActionMap.end()) {
            logger->Error("Invalid Action '%s'; not found");
            return false;
        }

        auto action = strToActionMap.at(actionName);

        if (value.IsScalar()) {
            // Action is mapped to a single key-combo
            auto keyPressCombo = value.Scalar();
            logger->Debug("Parsing: '%s' = '%s'", actionName.c_str(), keyPressCombo.c_str());

            if (!ParseKeyPressCombinationString(action, keyPressCombo, keymapModifiers)) {
                logger->Error("KeyMap parse error for '%s : %s'", actionName.c_str(), keyPressCombo.c_str());
                return false;
            }

        } else if (value.IsSequence()) {
            // Action is a sequence of different possible key-mappings...
            auto actionName = key.Scalar();
            auto sequence = value.as<std::vector<std::string>>();
            for(auto &keyPressCombo : sequence) {
                logger->Debug("Parsing: '%s' = '%s'", actionName.c_str(), keyPressCombo.c_str());

                if (!ParseKeyPressCombinationString(action, keyPressCombo, keymapModifiers)) {
                    logger->Error("KeyMap parse error for '%s : %s'", actionName.c_str(), keyPressCombo.c_str());
                    return false;
                }
            }
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
    for(auto &[name, strModifierMask] : keymapModifiers) {
        if (strToModifierMap.find(name) == strToModifierMap.end()) {
            logger->Error("No modifier named '%s'", name.c_str());
            return false;
        }
        if (!Keyboard::IsNameModifierMask(strModifierMask)) {
            logger->Error("No keycode named '%s' for modifier '%s'", strModifierMask.c_str(), name.c_str());
            return false;
        }
        logger->Debug("  '%s' = '%s'", name.c_str(), strModifierMask.c_str());
        auto modifier = strToModifierMap.at(name);
        modifiers[modifier] = Keyboard::ModifierMaskFromString(strModifierMask);
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
            tempKeyMap.keyCodeName = keycodeName;
        } else {
            // play keycode...
            keycodeName = s;
        }

        // Substitue with a modifier (if any)
        if (keymapModifiers.find(keycodeName) != keymapModifiers.end()) {
            logger->Debug("  Modifier (%s) => %s", keycodeName.c_str(), keymapModifiers.at(keycodeName).c_str());
            // Save this action modifier first before substituting
            tempKeyMap.actionModifier = strToModifierMap[keycodeName];
            keycodeName = keymapModifiers.at(keycodeName);
        }

        // check if the keycode is a modifier
        if (Keyboard::IsNameModifierMask(keycodeName)) {
            tempKeyMap.isModifier = true;
            tempKeyMap.modifiermask = Keyboard::ModifierMaskFromString(keycodeName);
        } else if (Keyboard::IsNameKeyCode(keycodeName)) {
            primaryKeycode = Keyboard::NameToKeyCode(keycodeName).value();
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
            tempKeyMap.modifiermask = Keyboard::ModifierMaskFromString(keycodeName);
            keycodePermutationList.push_back(tempKeyMap);
        }
    }
    // Dump:
    logger->Debug("  KeyCode: %s", Keyboard::KeyCodeName(primaryKeycode).c_str());

    for(auto &tempKeyMap : keycodePermutationList) {
        logger->Debug("    0x%.2x : [%s, %s]", tempKeyMap.modifiermask,
                      tempKeyMap.isModifier?"modifier":"regular",
                      tempKeyMap.isOptional?"optional":"required");

    }



    // permutations are 2^numOptional - it's binary...
    for(int i=0;i<pow(2, numOptional);i++) {
        int modifierMask = 0;
        std::optional<kActionModifier> actionModifier = {};
        for(auto &tempKeyMap : keycodePermutationList) {
            if (!tempKeyMap.isOptional) {
                modifierMask |= tempKeyMap.modifiermask;
            } else {
                // Magic goes here...
                // Each optional is assigned an on/off bit (permutationMask) if set we multiply the mask with '1' (i.e. the mask is added)
                // otherwise 0, the mask is not added (something | 0 = something)
                if (i & tempKeyMap.permutationMask) {
                    modifierMask |= (tempKeyMap.modifiermask * ((i & tempKeyMap.permutationMask) ? 1 : 0));
                    if (tempKeyMap.actionModifier.has_value()) {
                        actionModifier = tempKeyMap.actionModifier;
                    }
                }
            }
        }
        logger->Debug("  %d : %s + 0x%.2x", i, Keyboard::KeyCodeName(primaryKeycode).c_str(), modifierMask);

        ActionItem::Ref actionItem = {};
        if (isKeyCodeASCII) {
            actionItem =ActionItem::Create(action, modifierMask, asciiKeyCode, ActionName(action));
        } else {
            actionItem = ActionItem::Create(action, modifierMask, primaryKeycode, ActionName(action));
            if (actionModifier.has_value()) {
                actionItem->SetActionModifier(actionModifier.value());
            }
        }
        actionItems.push_back(actionItem);
    }

    return true;
}
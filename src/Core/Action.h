//
// Created by gnilk on 19.03.23.
//

#ifndef EDITOR_ACTION_H
#define EDITOR_ACTION_H

#include <memory>
#include <unordered_map>
#include <functional>
#include <string>

#include "KeyPress.h"
#include "KeyCodes.h"

namespace gedit {
    enum class kAction {
        kActionNone,
        kActionPageUp,              // default: PageUp
        kActionPageDown,            // default: PageDown
        kActionLineUp,
        kActionLineDown,
        kActionLineHome,
        kActionLineEnd,
        kActionLineStepSingleLeft,  // default: left arrow
        kActionLineStepSingleRight, // default: right arrow
        kActionLineWordLeft,
        kActionLineWordRight,
        kActionCommitLine,          // default: return
        kActionBufferStart,
        kActionBufferEnd,
        kActionGotoFirstLine,           // default: CMD+Home
        kActionGotoLastLine,           // default: CMD+Home
        kActionGotoTopLine,             // Default: cmd + pageUp
        kActionGotoBottomLine,          // Default: cmd + pageDown
        kActionCycleActiveView,         // Default: esc
        kActionCycleActiveEditor,       // TEST-TEST Default: F3
        // Edit actions
        kActionEditBackspace,
    };

    class ActionItem {
    public:
        using Ref = std::shared_ptr<ActionItem>;
    public:
        ActionItem()  = default;
        ActionItem(kAction mAction, int mModiferMask, Keyboard::kKeyCode mKeycode, std::string &actionName)
                : action(mAction), modiferMask(mModiferMask), keyCode(mKeycode), name(actionName) {
        }

        static ActionItem::Ref Create(kAction mAction, int mModiferMask, Keyboard::kKeyCode mKeycode, std::string &actionName) {
            return std::make_shared<ActionItem>(mAction, mModiferMask, mKeycode, actionName);
        }

        // Not sure about this one..
        bool MatchKeyPress(const KeyPress &keyPress) {
            //if (!keyPress.isHwEventValid) return false;
            if (keyCode == Keyboard::kKeyCode_PageUp) {
                int breakme = 1;
            }
            if ((modiferMask != 0) && (keyPress.modifiers == 0)) {
                return false;
            }
            if ((keyPress.modifiers & modiferMask) != keyPress.modifiers) {
                return false;
            }



            // TODO: if 'isSpecialKey' is false - then we should check if the 'isHwEventVali == true' && hwEvent.translatedKeyCode == 'asciiCode'

            if (!keyPress.isSpecialKey) return false;
            if (keyCode != keyPress.specialKey) return false;

            return true;
        }

        virtual ~ActionItem() = default;

        kAction GetAction() {
            return action;
        }

        const std::string &Name() {
            return name;
        }
        const std::string &ToString() {
            // FIXME: return the string combination from the modifiers + the keycode (use hashtables)
            return name;
        }
    private:
        kAction action = kAction::kActionNone;
        int modiferMask = 0;
        Keyboard::kKeyCode keyCode;
        std::string name;
        KeyPress keyPress = {};      // Keypress that caused this action
    };

    class ActionDispatch {
    public:
        using ActionDelegate = std::function<void(kAction action)>;
    public:
        ActionDispatch() = default;
        explicit ActionDispatch(ActionDelegate callbackAction) : cbAction(callbackAction) {

        }
        virtual ~ActionDispatch() = default;
    private:
        ActionDelegate cbAction = nullptr;
    };
}
#endif //EDITOR_ACTION_H

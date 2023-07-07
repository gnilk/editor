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
#include "Keyboard.h"

namespace gedit {
    enum class kAction {
        kActionNone,
        kActionPageUp,              // default: PageUp
        kActionPageDown,            // default: PageDown
        kActionLineUp,
        kActionLineDown,
        kActionLineHome,
        kActionLineEnd,
        kActionLineLeft,  // default: left arrow
        kActionLineRight, // default: right arrow
        kActionLineWordLeft,
        kActionLineWordRight,
        kActionCommitLine,          // default: return
        kActionBufferStart,
        kActionBufferEnd,
        kActionGotoFirstLine,           // default: CMD+Home
        kActionGotoLastLine,           // default: CMD+Home
        kActionGotoTopLine,             // Default: cmd + pageUp
        kActionGotoBottomLine,          // Default: cmd + pageDown
        kActionGotoLine,          // Quick cntrl command
        kActionCycleActiveView,         // Default: esc
        kActionCycleActiveViewNext,     // Default: Left_CMD + ]
        kActionCycleActiveViewPrev,     // Default: Left_CMD + [
        kActionCycleActiveEditor,       // TEST-TEST Default: F3
        kActionCycleActiveBufferNext,
        kActionCycleActiveBufferPrev,
        kActionEnterCommandMode,
        kActionLeaveCommandMode,
        kActionStartSearch,
        kActionLastSearch,
        kActionNextSearchResult,
        kActionPrevSearchResult,
        // Edit actions
        kActionEditBackspace,
        // Other
        kActionCopyToClipboard,
        kActionPasteFromClipboard,
        kActionInsertLineComment,
        // Modals
        kActionCloseModal,

    };

    class ActionItem {
    public:
        using Ref = std::shared_ptr<ActionItem>;
    public:
        ActionItem()  = default;
        ActionItem(kAction mAction, int mModiferMask, Keyboard::kKeyCode mKeycode, const std::string &actionName)
                : action(mAction), modiferMask(mModiferMask), keyCode(mKeycode), name(actionName) {
        }
        ActionItem(kAction mAction, int mModiferMask, int asciiValue, const std::string &actionName)
                : action(mAction), modiferMask(mModiferMask), asciiKeyCode(asciiValue), name(actionName) {
        }

        static ActionItem::Ref Create(kAction mAction, int mModiferMask, Keyboard::kKeyCode mKeycode, const std::string &actionName) {
            return std::make_shared<ActionItem>(mAction, mModiferMask, mKeycode, actionName);
        }

        static ActionItem::Ref Create(kAction mAction, int mModiferMask, int asciiValue, const std::string &actionName) {
            return std::make_shared<ActionItem>(mAction, mModiferMask, asciiValue, actionName);
        }
        virtual ~ActionItem() = default;

        bool IsShift() {
            return (modiferMask & (Keyboard::kMod_LeftShift | Keyboard::kMod_RightShift));
        }
        bool IsCtrl() {
            return (modiferMask & (Keyboard::kMod_LeftCtrl | Keyboard::kMod_RightCtrl));
        }
        bool IsCommand() {
            return (modiferMask & (Keyboard::kMod_LeftCommand | Keyboard::kMod_RightCommand));
        }

        // Not sure about this one..
        bool MatchKeyPress(const KeyPress &keyPress) {
            if (name == "GotoFirstLine" && (modiferMask == 0xc3)) {
                int breakme = 1;
            }
            if ((modiferMask != 0) && (keyPress.modifiers == 0)) {
                return false;
            }

            // 2023-05-25, gnilk, moved this to be _BEFORE_ the modifier check
            //                    this allows ASCII commands like '?' to be detected even if SHIFT needs to be pressed

            // If not special key is pressed, we check if the 'key' is valid and match against asciiKeyCode..
            if (!keyPress.isSpecialKey && keyPress.isKeyValid) {
                return (keyPress.key == asciiKeyCode);
            }

            if (IsShift() && !keyPress.IsShiftPressed()) {
                return false;
            }
            if (IsCtrl() && !keyPress.IsCtrlPressed()) {
                return false;
            }
            if (IsCommand() && !keyPress.IsCommandPressed()) {
                return false;
            }
            if (!IsCommand() && keyPress.IsCommandPressed()) {
                return false;
            }
            if (!IsShift() && keyPress.IsShiftPressed()) {
                return false;
            }
            if (!IsCtrl() && keyPress.IsCtrlPressed()) {
                return false;
            }



            // This doesn't work - will send false positives
            // if keypress holds SHIFT and the modifier is CTRL + SHIFT this will return true (obviously)
            // the modifier-mask of the action also could be ANY shift key
//            if ((keyPress.modifiers & modiferMask) != keyPress.modifiers) {
//                return false;
//            }

            if (keyCode != keyPress.specialKey) return false;

            return true;
        }


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
        int asciiKeyCode = 0;
        Keyboard::kKeyCode keyCode = Keyboard::kKeyCode_None;
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

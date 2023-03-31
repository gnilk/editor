//
// Created by gnilk on 24.03.23.
//

#ifndef EDITOR_KEYMAPPING_H
#define EDITOR_KEYMAPPING_H

#include <vector>
#include <optional>

#include "KeyCodes.h"
#include "Action.h"

namespace gedit {
    struct KeyPressAction {
        kAction action = kAction::kActionNone;     // Key press was mapped to this action
        KeyPress keyPress = {};  // Underlying keypress
    };
    class KeyMapping {
    public:
        virtual ~KeyMapping() = default;
        static KeyMapping &Instance();

        const std::string &KeyCodeName(const Keyboard::kKeyCode keyCode);
        const std::string &ActionName(const kAction action);
        int ModifierMaskFromString(const std::string &strModifiers);
        kAction ActionFromName(const std::string &strAction);
        std::optional<KeyPressAction> ActionFromKeyPress(const KeyPress &keyPress);
        bool RebuildActionMapping();
        bool IsInitialized() {
            return isInitialized;
        }
    private:
        KeyMapping() = default;
        bool Initialize();
        bool isInitialized = false;
        std::vector<ActionItem::Ref> actionItems = {};
    };
}


#endif //EDITOR_KEYMAPPING_H

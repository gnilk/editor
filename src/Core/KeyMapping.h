//
// Created by gnilk on 24.03.23.
//

#ifndef EDITOR_KEYMAPPING_H
#define EDITOR_KEYMAPPING_H

#include <vector>
#include <map>
#include <optional>

#include "KeyCodes.h"
#include "Action.h"

namespace gedit {
    struct KeyPressAction {
        kAction action = kAction::kActionNone;     // Key press was mapped to this action
        KeyPress keyPress = {};  // Underlying keypress
    };

    enum class kModifier {
        kModifierSelection,
        kModifierCopyPaste,
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
        bool RebuildActionMappingOld();
        bool RebuildActionMapping();

        bool IsInitialized() {
            return isInitialized;
        }

        std::optional<int> MaskForModifier(kModifier modifier) {
            if (modifiers.find(modifier) == modifiers.end()) {
                return {};
            }
            return modifiers[modifier];
        }

        bool HasActionModifier(const ActionItem::Ref action, kModifier modifier) {
            return false;
        }
    protected:
        bool ParseKeyPressCombinationString(const std::string &actionName, const std::string &keyPressCombo, const std::map<std::string, std::string> &keymapModifiers);
        bool ParseModifiers(const std::map<std::string, std::string> &keymapModifiers);
    private:
        KeyMapping() = default;
        bool Initialize();
        bool isInitialized = false;
        std::vector<ActionItem::Ref> actionItems = {};
        std::map<kModifier, int> modifiers = {};

    };
}


#endif //EDITOR_KEYMAPPING_H

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

    // TODO: Consider renaming/moving this whole thing (incl. the 'Action' stuff)

    //
    // This defines the keymapping modifiers...
    // A modifier is an action overlaid on the keypress as defined through the configuration file
    //
    enum class kModifier {
        kModifierSelection,
        kModifierCopyPaste,
        kModifierUINavigation,
    };

    //
    // When a keypress is given this is given to the UI from the ActionFromKeyPress
    //
    struct KeyPressAction {
        kAction action = kAction::kActionNone;     // Key press was mapped to this action
        std::optional <kModifier> modifier = {};
        int modifierMask = {};
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
        bool RebuildActionMappingOld();
        bool RebuildActionMapping();

        bool IsInitialized() {
            return isInitialized;
        }

        std::optional<kModifier> ModifierFromMask(int modifierMask) {
            if (modifierMask == 0) return {};
            for (auto &[modifier, mask] : modifiers) {
                if ((mask & modifierMask) == modifierMask) {
                    return modifier;
                }
            }
            return {};
        }

        std::optional<int> MaskForModifier(kModifier modifier) {
            if (modifiers.find(modifier) == modifiers.end()) {
                return {};
            }
            return modifiers[modifier];
        }

        const std::string &ModifierName(kModifier modifier);

    protected:
        bool ParseKeyPressCombinationString(kAction action, const std::string &keyPressCombo, const std::map<std::string, std::string> &keymapModifiers);
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

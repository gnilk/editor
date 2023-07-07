//
// Created by gnilk on 24.03.23.
//

#ifndef EDITOR_KEYMAPPING_H
#define EDITOR_KEYMAPPING_H

#include <vector>
#include <map>
#include <optional>

#include "Keyboard.h"
#include "Action.h"

namespace gedit {

    // TODO: Consider renaming/moving this whole thing (incl. the 'Action' stuff)

    //
    // This defines the keymapping modifiers...
    // A modifier is an action overlaid on the keypress as defined through the configuration file
    //
    // RENAME THIS - Modifiers = CTRL/ALT/SHIFT/CMD/etc..  this is just causing confusion!!!
    //
    enum class kActionModifier {
        kActionModifierSelection,
        kActionModifierCopyPaste,
        kActionModifierUINavigation,
    };

    //
    // When a keypress is given this is given to the UI from the ActionFromKeyPress
    //
    struct KeyPressAction {
        kAction action = kAction::kActionNone;     // Key press was mapped to this action
        std::optional <kActionModifier> actionModifier = {};
        int modifierMask = {};
        KeyPress keyPress = {};  // Underlying keypress
    };


    class KeyMapping {
    public:
        KeyMapping() = default;
        virtual ~KeyMapping() = default;

        bool Initialize(const std::string &cfgNodeName);


//        static KeyMapping &Instance();

        const std::string &ActionName(const kAction action);
        kAction ActionFromName(const std::string &strAction);
        std::optional<KeyPressAction> ActionFromKeyPress(const KeyPress &keyPress);
        bool RebuildActionMapping(const std::string &cfgNodeName);

        bool IsInitialized() {
            return isInitialized;
        }

        std::optional<kActionModifier> ActionModifierFromMask(int modifierMask) {
            if (modifierMask == 0) return {};
            for (auto &[modifier, mask] : modifiers) {
                // This will hit in priority order, we only allow one modifier per key-stroke
                // IF a keymap is declared like: CMD+HOME+@Selection (which is CMD+HOME+SHIFT) we will take SHIFT as the modifier of this action
                // FIXME: A key-action should explicitly declare the modifier and store it
                //        ONLY allow one modifier per action!!
                if (mask & modifierMask) {
                    return modifier;
                }
            }
            return {};
        }

        std::optional<int> MaskForModifier(kActionModifier modifier) {
            if (modifiers.find(modifier) == modifiers.end()) {
                return {};
            }
            return modifiers[modifier];
        }

        const std::string &ModifierName(kActionModifier modifier);

    protected:
        bool ParseKeyPressCombinationString(kAction action, const std::string &keyPressCombo, const std::map<std::string, std::string> &keymapModifiers);
        bool ParseModifiers(const std::map<std::string, std::string> &keymapModifiers);

    private:

    private:
        bool isInitialized = false;
        std::vector<ActionItem::Ref> actionItems = {};
        std::map<kActionModifier, int> modifiers = {};

    };
}


#endif //EDITOR_KEYMAPPING_H

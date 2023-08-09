//
// Created by gnilk on 24.03.23.
//

#ifndef EDITOR_KEYMAPPING_H
#define EDITOR_KEYMAPPING_H

#include <vector>
#include <map>
#include <optional>

#include "Core/Config/Config.h"
#include "Keyboard.h"
#include "Action.h"

namespace gedit {


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
        using Ref = std::shared_ptr<KeyMapping>;
    public:
        KeyMapping() = default;
        virtual ~KeyMapping() = default;

        static Ref Create(const std::string &cfgNodeName);
        static Ref Create(const ConfigNode &cfgNode);

        bool Initialize(const std::string &cfgNodeName);
        bool Initialize(const ConfigNode &cfgNode);

        const std::string &ActionName(const kAction action);
        kAction ActionFromName(const std::string &strAction);
        std::optional<KeyPressAction> ActionFromKeyPress(const KeyPress &keyPress);
        bool RebuildActionMapping(const std::string &cfgNodeName);
        bool RebuildActionMapping(const ConfigNode &cfgNode);

        bool IsInitialized() {
            return isInitialized;
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

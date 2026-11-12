//
// Created by gnilk on 24.03.23.
//

#ifndef EDITOR_KEYMAPPING_H
#define EDITOR_KEYMAPPING_H

#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <string>
#include <functional>

#include "Core/Config/Config.h"
#include "Keyboard.h"
#include "Action.h"
#include "EditorAction.h"

namespace gedit {


    class KeyMapping {
    public:
        using Ref = std::shared_ptr<KeyMapping>;
        // Resolves an inherited parent keymap (by name) to a fully-built KeyMapping. Supplied by the
        // KeyMappingCache so inheritance reuses cached parents; null means "no cache" (the legacy
        // direct-load walk is used instead - keeps memory-built keymaps hermetic).
        using ParentResolver = std::function<Ref(const std::string &)>;
    public:
        KeyMapping() = default;
        virtual ~KeyMapping() = default;

        static Ref Create(const std::string &cfgNodeName);
        static Ref Create(const ConfigNode &cfgNode);
        static Ref Create(const ConfigNode &cfgNode, const ParentResolver &resolveParent);

        // Loads the 'keymap' config node for a named keymap from the assets (OS-specific lookup).
        static std::optional<ConfigNode> LoadKeymapConfig(const std::string &name);

        bool Initialize(const std::string &cfgNodeName);
        bool Initialize(const ConfigNode &cfgNode);
        bool Initialize(const ConfigNode &cfgNode, const ParentResolver &resolveParent);

        const std::string &ActionName(const kAction action);
        kAction ActionFromName(const std::string &strAction);
        std::optional<EditorAction> ActionFromKeyPress(const KeyPress &keyPress);
        bool RebuildActionMapping(const std::string &cfgNodeName);
        bool RebuildActionMapping(const ConfigNode &cfgNode);

        bool IsInitialized() {
            return isInitialized;
        }

        const std::string &ModifierName(kActionModifier modifier);
        // The keys a named modifier stands for in this keymap (e.g. CopyPasteModifier -> Cmd on macOS, Ctrl on Linux) as
        // a Keyboard modifier mask; a child keymap's 'modifiers' win over its parent's. Empty when no keymap in the chain
        // defines it. Lets a mouse click with a modifier follow the keymap instead of a hard-coded key.
        std::optional<int> ModifierMask(kActionModifier modifier) const;

    protected:
        bool ResolveInheritance(const ConfigNode &cfgNode, const ParentResolver &resolveParent);
        bool ParseKeyPressCombinationString(const std::string &actionName, const std::string &keyPressCombo, const std::map<std::string, std::string> &keymapModifiers);
        bool ValidateModifiers(const std::map<std::string, std::string> &keymapModifiers);
        void AddModifierMasks(const std::map<std::string, std::string> &keymapModifiers);
    private:
        bool isInitialized = false;
        std::vector<ActionItem::Ref> actionItems = {};
        // From the 'modifiers' tables: own first, then the inherited ones (first one in wins)
        std::map<kActionModifier, int> modifierMasks = {};

    };
}


#endif //EDITOR_KEYMAPPING_H

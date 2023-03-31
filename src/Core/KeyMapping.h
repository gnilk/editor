//
// Created by gnilk on 24.03.23.
//

#ifndef EDITOR_KEYMAPPING_H
#define EDITOR_KEYMAPPING_H

#include "KeyCodes.h"
#include "Action.h"
#include <vector>

namespace gedit {
    class KeyMapping {
    public:
        virtual ~KeyMapping() = default;
        static KeyMapping &Instance();

        const std::string &KeyCodeName(const Keyboard::kKeyCode keyCode);
        const std::string &ActionName(const kAction action);
        int ModifierMaskFromString(const std::string &strModifiers);
        kAction ActionFromName(const std::string &strAction);
        kAction ActionFromKeyPress(const KeyPress &keyPress);
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

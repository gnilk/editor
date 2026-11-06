//
// Created by gnilk on 16.05.23.
//

#ifndef EDITOR_ACTIONHELPER_H
#define EDITOR_ACTIONHELPER_H

#include <cstdint>
#include <string>

#include "Core/Index/GoToResolver.h"

namespace gedit {
    class ActionHelper {
    public:
        static void SwitchToNextBuffer();
        static void SwitchToPreviousBuffer();
        // Tab 'tabNumber' (1..9, 9 = last) becomes the active buffer and the editor gets focus
        static void SwitchToBufferNumber(int tabNumber);
        static void SwitchToNamedView(const std::string &viewName);
        static void ShowFind();
        static void ShowGotoSymbol();
        // Smart go-to on the name under the cursor: one place jumps, several open a list (docs/code-index.md §5.6)
        static void GoToNameAtCursor(GoToResolver::kMode mode);
        // Opens the place and puts the jump on the navigation stack
        static bool JumpTo(const std::string &path, uint32_t line, uint32_t column);
        static void JumpBack();
        static void JumpForward();
    };
}


#endif //EDITOR_ACTIONHELPER_H

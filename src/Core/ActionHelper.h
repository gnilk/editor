//
// Created by gnilk on 16.05.23.
//

#ifndef EDITOR_ACTIONHELPER_H
#define EDITOR_ACTIONHELPER_H

#include <string>

namespace gedit {
    class ActionHelper {
    public:
        static void SwitchToNextBuffer();
        static void SwitchToPreviousBuffer();
        // Tab 'tabNumber' (1..9, 9 = last) becomes the active buffer and the editor gets focus
        static void SwitchToBufferNumber(int tabNumber);
        static void SwitchToNamedView(const std::string &viewName);
    };
}


#endif //EDITOR_ACTIONHELPER_H

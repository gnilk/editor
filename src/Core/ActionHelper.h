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
        static void SwitchToNamedView(const std::string &viewName);
    };
}


#endif //EDITOR_ACTIONHELPER_H

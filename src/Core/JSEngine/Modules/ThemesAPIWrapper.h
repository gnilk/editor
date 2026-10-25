//
// JS bridge for ThemesAPI - a global 'Themes' object (like 'Config'). Every call returns the lines to show.
//

#ifndef EDITOR_THEMESAPIWRAPPER_H
#define EDITOR_THEMESAPIWRAPPER_H

#include <string>
#include <vector>
#include "duktape.h"

namespace gedit {
    class ThemesAPIWrapper {
    public:
        static void RegisterModule(duk_context *ctx);

        std::vector<std::string> Import(const char *file, const char *name, bool isForced);
    };
}

#endif //EDITOR_THEMESAPIWRAPPER_H

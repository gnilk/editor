//
// JS bridge for ConfigAPI - a global 'Config' object (like 'Console'). Levels are passed as strings:
// "" (running), "user" or "project". Every call returns the lines to show.
//

#ifndef EDITOR_CONFIGAPIWRAPPER_H
#define EDITOR_CONFIGAPIWRAPPER_H

#include <string>
#include <vector>
#include "duktape.h"

namespace gedit {
    class ConfigAPIWrapper {
    public:
        static void RegisterModule(duk_context *ctx);

        std::vector<std::string> List(const char *level, const char *prefix);
        std::vector<std::string> Get(const char *level, const char *key);
        std::vector<std::string> Set(const char *level, const char *key, const char *value);
        std::vector<std::string> Edit(const char *level);
    };
}

#endif //EDITOR_CONFIGAPIWRAPPER_H

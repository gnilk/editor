//
// Created by gnilk on 27.03.23.
//

#ifndef GEDIT_EMBDUKTAPEJS_JSWRAPPER_H
#define GEDIT_EMBDUKTAPEJS_JSWRAPPER_H

#include "duktape.h"
#include <string>
#include <vector>

namespace gedit {
    class JSWrapper {
    public:
        bool Initialize();
        bool RunScriptOnce(const std::string &script, std::vector<std::string> &args);
        duk_context *GetContext() {
            return ctx;
        }
    protected:
        bool ConfigureNodeModuleSupport();
        void RegisterBuiltIns();
    private:
        duk_context *ctx;
    };
}

#endif //GEDIT_EMBDUKTAPEJS_JSWRAPPER_H

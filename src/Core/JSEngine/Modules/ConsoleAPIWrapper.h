//
// Created by gnilk on 04.05.23.
//

#ifndef EDITOR_CONSOLEAPIWRAPPER_H
#define EDITOR_CONSOLEAPIWRAPPER_H

#include "duktape.h"

namespace gedit {
    class ConsoleAPIWrapper {
    public:
        duk_ret_t WriteLine(duk_context *ctx);
        // §5.5.3: returns the terminal's selected command-block output as a JS string, or null when
        // nothing is selected (following the live bottom / no block concept). Routes via
        // RuntimeConfig::OutputConsole()->GetSelectedBlockText().
        duk_ret_t GetSelectedBlock(duk_context *ctx);
        static void RegisterModule(duk_context *ctx);
    private:
        void SendToConsole(const char *str);
    };
}


#endif //EDITOR_CONSOLEAPIWRAPPER_H

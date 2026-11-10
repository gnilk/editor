//
// Created by gnilk on 24.06.24.
//
// Duktape glue for TerminalAPI, registered as the JS `Terminal` global. Deliberately THIN - it only marshals
// between duktape values and TerminalAPI; the logic lives in TerminalAPI so swapping the scripting engine touches
// only this file. The methods hand-roll duk_ret_t because a result may be null.
//

#ifndef GOATEDIT_TERMINALAPIWRAPPER_H
#define GOATEDIT_TERMINALAPIWRAPPER_H

#include "duktape.h"

namespace gedit {
    class TerminalAPIWrapper {
    public:
        static void RegisterModule(duk_context *ctx);

        // Terminal.GetLastOutput() -> string | null
        duk_ret_t GetLastOutput(duk_context *ctx);
        // Terminal.OpenLastOutputAsDocument() -> bool (true when a document was opened)
        duk_ret_t OpenLastOutputAsDocument(duk_context *ctx);
        // Terminal.CopyLastOutputToClipboard() -> bool (true when it was copied)
        duk_ret_t CopyLastOutputToClipboard(duk_context *ctx);
    };
}

#endif //GOATEDIT_TERMINALAPIWRAPPER_H

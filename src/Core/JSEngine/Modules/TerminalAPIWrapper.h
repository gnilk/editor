//
// Created by gnilk on 24.06.24.
//
// Duktape glue for TerminalAPI (terminal-scrollback §9 / TS-2c). Registered as the JS `Terminal`
// global. This layer is deliberately THIN - it only marshals between duktape values and TerminalAPI;
// all logic lives in TerminalAPI (the engine-agnostic layer) so swapping the scripting engine touches
// only this file. Each method hand-rolls duk_ret_t because the results are arrays/objects/null that
// don't map cleanly onto a dukglue method signature.
//

#ifndef GOATEDIT_TERMINALAPIWRAPPER_H
#define GOATEDIT_TERMINALAPIWRAPPER_H

#include "duktape.h"

namespace gedit {
    class TerminalAPIWrapper {
    public:
        static void RegisterModule(duk_context *ctx);

        // Terminal.GetBlocks() -> [{ id, command, exitCode|null, lineCount }, ...] (oldest->newest).
        duk_ret_t GetBlocks(duk_context *ctx);
        // Terminal.GetBlockOutput(id) -> string | null.
        duk_ret_t GetBlockOutput(duk_context *ctx);
        // Terminal.GetLastBlock() -> { id, command, exitCode|null, lineCount } | null.
        duk_ret_t GetLastBlock(duk_context *ctx);
        // Terminal.GetSelectedBlock() -> string | null (the viewport-selected block's output).
        duk_ret_t GetSelectedBlock(duk_context *ctx);
        // Terminal.OpenBlockAsDocument(id) -> bool (true when a document was opened).
        duk_ret_t OpenBlockAsDocument(duk_context *ctx);
    };
}

#endif //GOATEDIT_TERMINALAPIWRAPPER_H

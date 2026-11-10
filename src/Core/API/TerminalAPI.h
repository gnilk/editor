//
// Created by gnilk on 24.06.24.
//
// Engine-agnostic terminal API: the last command's output (a stack trace, say) as text, in a new document or on
// the clipboard. The LOGIC layer of the two-layer API split; the thin duktape glue lives in TerminalAPIWrapper. It
// reaches the terminal through the IOutputConsole seam (RuntimeConfig::OutputConsole()), not TerminalController.
//

#ifndef GOATEDIT_TERMINALAPI_H
#define GOATEDIT_TERMINALAPI_H

#include <optional>
#include <string>

#include "Core/RuntimeConfig.h"
#include "DocumentAPI.h"

namespace gedit {
    class TerminalAPI {
    public:
        TerminalAPI() = default;
        virtual ~TerminalAPI() = default;

        // The last command's output, or nullopt when there is none (or no terminal console)
        std::optional<std::u32string> GetLastOutput();
        // It in a fresh, editable document; nullptr when there is no output
        DocumentAPI::Ref OpenLastOutputAsDocument();
        // It on the system clipboard; false when there is no output
        bool CopyLastOutputToClipboard();

    private:
        // The terminal seam, or nullptr when no output console is registered (headless / non-terminal).
        IOutputConsole *Console();
    };
}

#endif //GOATEDIT_TERMINALAPI_H

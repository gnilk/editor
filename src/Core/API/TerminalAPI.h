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
#include <vector>

#include "Core/RuntimeConfig.h"
#include "Core/Build/BuildMessages.h"
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

        // A build message as a script sees it (docs/build-messages.md §7 step 4)
        struct Message {
            BuildMessage message;
            std::string resolvedPath;       // the file it points at; empty when it isn't found
        };
        // The last build's errors and warnings, as the list and F8 have them (errors first); nullopt before the first
        // build. Read from the editor's BuildMessages - any terminal's last build, while it runs too.
        std::optional<std::vector<Message>> GetBuildMessages();

        // The panels' layout (hitlist #77): the setting and what it is now; set it ('portrait' | 'landscape' | 'auto')
        std::string GetLayout();
        bool SetLayout(const std::string &setting);
        // The focused panel's shown tab to the other panel - what happened, for the user
        std::string MoveTab();

    private:
        // The terminal seam, or nullptr when no output console is registered (headless / non-terminal).
        IOutputConsole *Console();
    };
}

#endif //GOATEDIT_TERMINALAPI_H

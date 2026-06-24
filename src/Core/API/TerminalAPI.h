//
// Created by gnilk on 24.06.24.
//
// Engine-agnostic terminal/command-block API (terminal-scrollback §9 / TS-2c). This is the LOGIC layer
// of the two-layer API split: it owns the block reads + the block->document composite, and is
// independent of any scripting engine. The thin duktape glue lives in TerminalAPIWrapper. It consumes
// the terminal through the IOutputConsole seam (RuntimeConfig::OutputConsole()) - it does NOT depend on
// TerminalController - so any console that exposes blocks works, and a different scripting engine could
// reuse it unchanged.
//

#ifndef GOATEDIT_TERMINALAPI_H
#define GOATEDIT_TERMINALAPI_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Core/RuntimeConfig.h"
#include "DocumentAPI.h"

namespace gedit {
    class TerminalAPI {
    public:
        using BlockInfo = IOutputConsole::OutputBlockInfo;

    public:
        TerminalAPI() = default;
        virtual ~TerminalAPI() = default;

        // Enumerate the terminal's command blocks oldest->newest (incl. the open/loose tail block).
        std::vector<BlockInfo> GetBlocks();
        // One block's output as joined text, or nullopt for an unknown id (or no terminal console).
        std::optional<std::u32string> GetBlockOutput(uint64_t blockId);
        // The newest block (the tail), or nullopt when there is no terminal console.
        std::optional<BlockInfo> GetLastBlock();
        // The viewport-selected block's output, or nullopt when nothing is selected (following bottom).
        std::optional<std::u32string> GetSelectedBlock();

        // Composite (the payoff, §9): resolve a block's output and drop it into a fresh, editable
        // Document. Returns nullptr when the id resolves to no output. This cross-API logic lives HERE,
        // in the engine-agnostic layer - not in the JS glue.
        DocumentAPI::Ref OpenBlockAsDocument(uint64_t blockId);

    private:
        // The terminal seam, or nullptr when no output console is registered (headless / non-terminal).
        IOutputConsole *Console();
    };
}

#endif //GOATEDIT_TERMINALAPI_H

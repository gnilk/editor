//
// Created by gnilk on 22.02.2024.
//

#ifndef GOATEDIT_TERMINALCONTROLLER_H
#define GOATEDIT_TERMINALCONTROLLER_H

#include <vector>
#include <string>
#include <mutex>

#include "logger.h"
#include "BaseController.h"
#include "Core/RuntimeConfig.h"
#include "Core/unix/Shell.h"
#include "Core/TerminalScreen.h"
#include "Core/VTermParser.h"

namespace gedit {
    class TerminalController : public BaseController, IOutputConsole {
    public:
        // Shell-mode line ownership:
        //  kLocalEdit  - we edit the line locally (inputLine); nothing reaches the
        //                shell until the line is committed. This is the default.
        //  kShellOwned - entered on the first Tab/ShellCompletion: the line is handed
        //                to the shell's readline so it can complete. Keys pass through
        //                to the pty and the grid is mirrored back into inputLine until
        //                the line is committed (or aborted), which returns to kLocalEdit.
        enum class TermMode {
            kLocalEdit,
            kShellOwned,
        };

    public:
        TerminalController() = default;
        virtual ~TerminalController() = default;

        void Begin() override;
        void Resize(int cols, int rows);
        bool HandleKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress) override;

        const TerminalScreen &GetScreen() const { return screen; }
        std::mutex &GetScreenLock() { return screenLock; }

        Line::Ref GetInputLine() const { return inputLine; }
        void CommitLine();
        int GetCursorXPos();

        bool IsShellOwned() const { return termMode == TermMode::kShellOwned; }

        bool OnAction(const KeyPressAction &kpAction);
        bool ForwardActionToShell(const KeyPressAction &kpAction);
        void WriteLine(const std::u32string &str) override;

    protected:
        void HandleTerminalData(const uint8_t *buffer, size_t length);
        void ApplyCommand(const VTermParser::CMD &cmd);
        void SyncInputLineFromGrid();
        void ExitShellOwned();
        void InitializeColorTable();

        bool HandleKeyPressAltScreen(const KeyPress &keyPress);

    private:
        Shell shell;
        gnilk::ILogger *logger = nullptr;

        Line::Ref inputLine = nullptr;
        Cursor inputCursor;

        TerminalScreen screen;
        std::mutex screenLock;

        bool cursorKeyAppMode = false;

        // Shell-mode line ownership state (see TermMode above).
        TermMode termMode = TermMode::kLocalEdit;
        // Grid cell where the prompt ends, captured on the kLocalEdit->kShellOwned
        // transition (the real grid cursor stays parked there while editing locally,
        // since local edits never touch the grid). Used to read the line back.
        Point promptAnchor = {};
    };
}

#endif //GOATEDIT_TERMINALCONTROLLER_H

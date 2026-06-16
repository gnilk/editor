//
// Created by gnilk on 22.02.2024.
//

#ifndef GOATEDIT_TERMINALCONTROLLER_H
#define GOATEDIT_TERMINALCONTROLLER_H

#include <vector>
#include <string>
#include <mutex>
#include <filesystem>

#include "logger.h"
#include "Core/UI/Controllers/BaseController.h"
#include "Core/RuntimeConfig.h"
#include "Core/unix/Shell.h"
#include "Core/TerminalScreen.h"
#include "Core/TerminalHistory.h"
#include "Core/VTermParser.h"

namespace gedit {
    class TerminalController : public BaseController, IOutputConsole {
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

        bool DoesShellOwnLineEditing() const { return doesShellOwnLineEditing; }

        bool OnAction(const EditorAction &kpAction);
        bool ForwardActionToShell(const EditorAction &kpAction);
        void WriteLine(const std::u32string &str) override;

    protected:
        void HandleTerminalData(const uint8_t *buffer, size_t length);
        void HandleAnsiCmd(const VTermParser::CMD &cmd);
        void SyncInputLineFromGrid();
        void ExitShellOwned();
        void InitializeColorTable();

        // Encode a keypress as raw pty bytes and forward it to the shell. Used by BOTH passthrough
        // paths in HandleKeyPress: a full-screen app (alt-screen) and readline (shell-owned line).
        bool ForwardKeyPressToShell(const KeyPress &keyPress);

    private:
        Shell shell;
        TerminalHistory history;
        std::filesystem::path historyPath;
        gnilk::ILogger *logger = nullptr;

        Line::Ref inputLine = nullptr;
        Cursor inputCursor;

        TerminalScreen screen;
        std::mutex screenLock;

        bool cursorKeyAppMode = false;

        // Line-ownership flag - ONE of two independent dimensions that decide how a keypress is
        // handled; the other is screen.IsAltScreen() (a flag on the TerminalScreen model, flipped by
        // ANSI alt-screen escapes - independent of this, NOT a sub-state of it). See HandleKeyPress
        // for how the two combine.
        //  false (default) - we edit the line locally (inputLine); nothing reaches the shell until
        //                    the line is committed.
        //  true            - entered on the first Tab/ShellCompletion: the line is handed to the
        //                    shell's readline so it can complete. Keys pass through to the pty and
        //                    the grid is mirrored back into inputLine until the line is committed
        //                    (or aborted), which returns to local editing.
        bool doesShellOwnLineEditing = false;
        // Grid cell where the prompt ends, captured on the local->shell-owned transition (the real
        // grid cursor stays parked there while editing locally, since local edits never touch the
        // grid). Used to read the line back.
        Point promptAnchor = {};
    };
}

#endif //GOATEDIT_TERMINALCONTROLLER_H

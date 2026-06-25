//
// Created by gnilk on 22.02.2024.
//

#ifndef GOATEDIT_TERMINALCONTROLLER_H
#define GOATEDIT_TERMINALCONTROLLER_H

#include <vector>
#include <string>
#include <mutex>
#include <filesystem>
#include <optional>

#include "logger.h"
#include "Core/UI/Controllers/BaseController.h"
#include "Core/RuntimeConfig.h"
#include "Core/unix/Shell.h"
#include "Core/TerminalScreen.h"
#include "Core/TerminalCmdHistory.h"
#include "Core/VTermParser.h"

namespace gedit {
    struct TerminalSession;   // Core/Session/SessionState.h - the persisted block index + .bin pointer

    class TerminalController : public BaseController, IOutputConsole {
    public:
        TerminalController() = default;
        virtual ~TerminalController() = default;

        void Begin() override;
        void Resize(int cols, int rows);
        bool HandleKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress) override;

        // The shell's pty-output sink: feed raw bytes here (Begin() wires it as Shell's output
        // delegate, called on the pty-reader thread). Parses ANSI/OSC and mutates the grid under
        // screenLock. Public so a test can drive a scripted byte stream (e.g. OSC 133 markers) into
        // the controller without a live shell - the same role RegisterAsOutputConsole plays for output.
        void HandleTerminalData(const uint8_t *buffer, size_t length);

        const TerminalScreen &GetScreen() const { return screen; }
        std::mutex &GetScreenLock() { return screenLock; }

        Line::Ref GetInputLine() const { return inputLine; }
        void CommitLine();
        int GetCursorXPos();

        bool DoesShellOwnLineEditing() const { return doesShellOwnLineEditing; }

        // Viewport (§5.1 of docs/terminal-scrollback.md): scroll position is a UI/viewport concern,
        // not model state - it lives here, not on TerminalScreen.
        bool IsFollowingBottom() const { return followBottom; }
        uint64_t GetAnchorAbsRow() const { return anchorAbsRow; }

        // Move the visible window by 'deltaRows' (positive = toward the live bottom/newer content,
        // negative = toward older history), clamped into [screen.ScrollbackBase(), screen.AbsRowCount()].
        // Landing exactly on the bottom snaps back to followBottom. A plain, independently-testable
        // mutator - TS-0f wires wheel/PageUp-Down gestures to it, nothing calls it yet.
        void ScrollViewport(int64_t deltaRows);

        // Explicit snap back to the live bottom. Wired to the two triggers in §5.3: committing a line
        // (CommitLine) and any local text input into inputLine (HandleKeyPress's local-edit path).
        // New output streaming in does NOT snap - so you can keep reading while output scrolls by.
        void ScrollToBottom();

        // Jump to the oldest retained row. Wired to kUIActionBufferStart (Ctrl+Home) in the
        // shell-prompt path - the backlog equivalent of "go to top".
        void ScrollToTop();

        // Jump-per-block (§5.4): set the anchor to the previous/next command block's startAbsRow,
        // relative to whatever's currently top-visible (the followBottom window-top, or anchorAbsRow
        // when already scrolled). Always leaves followBottom == false; clamped to the oldest/newest
        // block - never walks past either end of screen.Blocks() (which is never empty, TS-1a).
        void JumpToPrevPrompt();
        void JumpToNextPrompt();

        // Index into screen.Blocks() of the block "selected" by the viewport (§5.5.2) - the block
        // containing the top-visible anchor row while scrolled. nullopt when following the bottom (at
        // the live prompt) or when the anchor falls in no block. Drives the block highlight + action
        // bar. Assumes screenLock is held (the view holds it; single-threaded tests need none).
        std::optional<size_t> SelectedBlockIndex() const;

        bool OnAction(const EditorAction &kpAction);
        bool ForwardActionToShell(const EditorAction &kpAction);
        void WriteLine(const std::u32string &str) override;

        // Register this controller as the process-wide output console (RuntimeConfig::OutputConsole()) -
        // the IOutputConsole the JS Console/Terminal surfaces route to. IOutputConsole is a PRIVATE base
        // (external code reaches it only through RuntimeConfig, never via TerminalController*), so the
        // upcast must happen in here. Called from Begin(); exposed so a test can wire a controller up
        // without starting a real shell.
        void RegisterAsOutputConsole() { RuntimeConfig::Instance().SetOutputConsole(this); }

        // IOutputConsole (§5.5.3): the viewport-selected block's output joined into one string, or
        // nullopt when nothing is selected (following the live bottom). Takes screenLock, so it is safe
        // to call from the JS/main thread while the pty thread mutates the grid. The Terminal JS surface
        // routes here via RuntimeConfig::OutputConsole().
        std::optional<std::u32string> GetSelectedBlockText() override;

        // IOutputConsole block-by-id surface (TS-2c): enumerate blocks, resolve one block's output by
        // id, and the newest block. Each takes screenLock (the pty thread mutates the grid/index). The
        // JS `Terminal` surface routes here via RuntimeConfig::OutputConsole().
        std::vector<OutputBlockInfo> GetBlocks() override;
        std::optional<std::u32string> GetBlockOutputText(uint64_t blockId) override;
        std::optional<OutputBlockInfo> GetLastBlock() override;

        // Scrollback persistence (terminal-scrollback.md §8). Driven by the session orchestrator:
        // Editor::SaveSession calls ToSession (clean-exit only); restore runs inside Begin() before the
        // shell starts (FromSession). Both gate on `terminal.persist_scrollback` (on by default) and on a
        // project `.goatedit` dir (the .bin lives alongside session.yml); a no-op otherwise.
        //
        // ToSession snapshots the last `terminal.persist_scrollback_lines` scrollback lines under
        // screenLock, writes them (with ANSI colours) to the .bin OUTSIDE the lock via
        // TextBuffer::SaveWithAttributes, and fills `out` with the re-based block index. Returns false
        // (leaving `out` empty) while alt-screen, when scrollback is empty, or on a write failure.
        bool ToSession(TerminalSession &out);

        // FromSession loads the .bin (LoadWithAttributes - missing/corrupt -> no-op) and seeds
        // scrollback+blocks via TerminalScreen::SeedScrollback. MUST be called before shell.Begin().
        void FromSession(const TerminalSession &in);

    protected:
        void HandleAnsiCmd(const VTermParser::CMD &cmd);
        void SyncInputLineFromGrid();
        void ExitShellOwned();
        void InitializeColorTable();

        // Read the grid text spanning [from, to) cell positions, joining wrapped rows and trimming
        // trailing blanks. Used to recover the typed command from the grid span between OSC 133;B
        // (command start) and ;C (output start). Assumes screenLock is held.
        std::u32string ReadGridText(const Point &from, const Point &to) const;

        // Encode a keypress as raw pty bytes and forward it to the shell. Used by BOTH passthrough
        // paths in HandleKeyPress: a full-screen app (alt-screen) and readline (shell-owned line).
        bool ForwardKeyPressToShell(const KeyPress &keyPress);

        // Resolve the project-local path of the scrollback .bin (the kProject `.goatedit` write path for
        // `filename`), or empty when there is no project dir - the gate for project-scoped persistence.
        std::filesystem::path ResolveScrollbackBinPath(const std::string &filename) const;

        // The abs id of the row currently at the top of the visible window - the followBottom
        // window-top when pinned to the bottom, or anchorAbsRow itself when already scrolled. Shared
        // by ScrollViewport and the jump-per-block handlers so they agree on "where we are". Assumes
        // screenLock is already held.
        uint64_t CurrentTopVisibleAbsRow() const;

    private:
        Shell shell;
        TerminalCmdHistory cmdHistory;
        std::filesystem::path cmdHistoryPath;
        gnilk::ILogger *logger = nullptr;

        // Default-constructed (not nullptr) so the controller is usable in unit tests that never call
        // Begin() (no real shell, the established pattern - see WriteLine's doc comment). Begin()
        // re-creates it anyway for an explicit reset on a real session start.
        Line::Ref inputLine = std::make_shared<Line>();
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

        // OSC 133 shell-integration state (§4.2). Set once the first OSC 133 marker arrives: from then
        // on the shell brackets its own commands, so the CommitLine heuristic stands down (it would
        // otherwise double-open against ;C). Read under screenLock in the commit paths. Touched only on
        // the pty thread inside HandleAnsiCmd (already under screenLock).
        bool useOsc133Boundaries = false;
        // Grid cell recorded at OSC 133;B (command start); ;C reads the typed command from [here..cursor].
        Point osc133CommandStart = {};

        // Viewport state (§5.1) - read/written on the UI thread only (key/mouse handlers); reads of
        // screen.AbsRowCount()/ScrollbackBase() go through screenLock since the pty thread mutates them.
        bool     followBottom = true;
        uint64_t anchorAbsRow = 0;
    };
}

#endif //GOATEDIT_TERMINALCONTROLLER_H

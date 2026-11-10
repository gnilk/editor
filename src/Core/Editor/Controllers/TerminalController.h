//
// Created by gnilk on 22.02.2024.
//

#ifndef GOATEDIT_TERMINALCONTROLLER_H
#define GOATEDIT_TERMINALCONTROLLER_H

#include <functional>
#include <vector>
#include <string>
#include <mutex>
#include <filesystem>
#include <optional>
#include <chrono>

#include "logger.h"
#include "Core/UI/Controllers/BaseController.h"
#include "Core/RuntimeConfig.h"
#include "Core/unix/Shell.h"
#include "Core/TerminalScreen.h"
#include "Core/TerminalCmdHistory.h"
#include "gvt/Terminal.h"
#include "gvt/Input.h"

namespace gedit {
    struct TerminalSession;   // Core/Session/SessionState.h - the persisted block index + .bin pointer

    // The pty side of the embedded terminal: shell, local line editing, viewport, blocks. The emulation is
    // gnilk::vt::Terminal inside TerminalScreen; this class is its host (ITerminalHost) - it writes the terminal's
    // replies to the pty and turns OSC 133 / OSC 7 into command blocks.
    class TerminalController : public BaseController, IOutputConsole, gnilk::vt::ITerminalHost {
    public:
        TerminalController();
        virtual ~TerminalController() = default;

        void Begin() override;
        void Resize(int cols, int rows);
        bool HandleKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress) override;

        // The shell's pty-output sink: feed raw bytes here (Begin() wires it as Shell's output
        // delegate, called on the pty-reader thread). Feeds the terminal under screenLock; sequences
        // split across reads are fine. Public so a test can drive a scripted byte stream (e.g. OSC 133 markers) into
        // the controller without a live shell - the same role RegisterAsOutputConsole plays for output.
        void HandleTerminalData(const uint8_t *buffer, size_t length);

        const TerminalScreen &GetScreen() const { return screen; }
        // True while the app has synchronized output on (?2026, at most kMaxSyncHold): output is taken in but no
        // redraw is asked for until the app ends the update - the screen is never drawn half-way through a frame
        bool IsHoldingRedraw() const { return isHoldingRedraw; }
        std::mutex &GetScreenLock() { return screenLock; }
        // Alt+Left/Right to an app: ESC b / ESC f (readline's word motion, as Terminal.app/iTerm2 send) instead of
        // xterm's CSI 1;3D / 1;3C. Config 'terminal.alt_arrows: words | xterm'; macOS defaults to words.
        void SetAltArrowsAsWords(bool isWords) { isAltArrowsAsWords = isWords; }

        Line::Ref GetInputLine() const { return inputLine; }
        void CommitLine();
        int GetCursorXPos();

        bool DoesShellOwnLineEditing() const { return doesShellOwnLineEditing; }

        // An application owns the terminal: a full-screen app on the alternate screen, or any job in the foreground
        // (dialog, python, ssh, sleep, ...: the pty's foreground process group is not the shell's). Then every key
        // goes to the pty except the escape hatch (IsTerminalEscapeHatch), and the view shows the grid as is.
        bool IsAppActive();
        // The editor keeps these while an app owns the terminal: view focus/cycling/resizing, the
        // view modes, paste (sent to the app as a paste) and anything with Cmd. Everything else reaches the app.
        static bool IsTerminalEscapeHatch(const EditorAction &kpAction);

        // Clipboard text into the terminal. An app (or readline, while it owns the line) gets it as a paste
        // (bracketed when it asked, ?2004). At the local prompt one line is inserted at the caret; several lines go
        // to readline as one bracketed paste with the typed line - shown there for review, not run.
        void Paste(const std::u32string &text);
        // Focus in/out for apps that asked for focus events (?1004)
        void SetFocus(bool isFocused);
        // A mouse event at a grid cell, for an app that turned on mouse reporting. False when nothing was sent
        // (reporting off, or not an event the mode reports) - the view then handles it itself.
        bool ForwardMouse(gnilk::vt::MouseButton button, gnilk::vt::MouseAction action, int x, int y,
                          uint8_t modifiers);
        // The terminal's modes (DECCKM, bracketed paste, mouse, ...) - a copy taken under screenLock
        gnilk::vt::Modes GetModes();

        // Test seams: where pty bytes go (default: the shell), and the foreground-job probe (default:
        // Shell::IsForegroundJobRunning)
        void SetPtyWriter(std::function<void(std::string_view)> writer) { ptyWriter = std::move(writer); }
        void SetForegroundJobProbe(std::function<bool()> probe) { foregroundJobProbe = std::move(probe); }

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

        // gnilk::vt::ITerminalHost - called by the terminal inside HandleTerminalData, screenLock held
        void WriteToPty(std::string_view bytes) override;
        void OnSemanticPrompt(char kind, std::string_view params) override;
        void OnWorkingDirectory(std::string_view uri) override;

    protected:
        void SyncInputLineFromGrid();
        void ExitShellOwned();
        void RecordCommand(const std::u32string &cmdLine);
        // Default fg/bg and the 16 system colours from the theme's terminal colours (the rest of the
        // 256-colour palette is xterm's). Caller holds screenLock.
        void ApplyThemeColors();

        // Read the grid text spanning [from, to) cell positions, joining wrapped rows and trimming
        // trailing blanks. Used to recover the typed command from the grid span between OSC 133;B
        // (command start) and ;C (output start). Assumes screenLock is held.
        std::u32string ReadGridText(const Point &from, const Point &to) const;

        // Encode a keypress as pty bytes (gvt EncodeKey/EncodeText over the current modes) and send it. Used by
        // every passthrough path: an app owns the terminal, or readline owns the line. False for keys that never
        // go to the pty (Cmd combinations, keys with no encoding).
        bool ForwardKeyPressToShell(const KeyPress &keyPress);

        void LoadCmdHistory();
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

        std::function<void(std::string_view)> ptyWriter;
        std::function<bool()> foregroundJobProbe;

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
        // Synchronized output (?2026): when the app began the update; held for at most kMaxSyncHold, as Ghostty
        static constexpr std::chrono::milliseconds kMaxSyncHold{1000};
        std::optional<std::chrono::steady_clock::time_point> syncStart;
        bool isHoldingRedraw = false;
#ifdef GEDIT_MACOS
        bool isAltArrowsAsWords = true;
#else
        bool isAltArrowsAsWords = false;
#endif
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

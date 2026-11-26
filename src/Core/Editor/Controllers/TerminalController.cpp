//
// Created by gnilk on 22.02.2024.
//

#include <algorithm>
#include <fmt/format.h>

#include "TerminalController.h"
#include "Core/Editor.h"
#include "Core/HexDump.h"
#include "Core/Plugins/PluginExecutor.h"
#include "Core/UnicodeHelper.h"
#include "Core/Session/SessionManager.h"
#include "Core/Session/SessionState.h"
#include "Core/InstallPaths.h"
#include "Core/XDGEnvironment.h"
#include "Core/Runloop.h"

using namespace gedit;

// OSC 133 shell-integration prompt hooks (TS-3c, §4.2), injected into the shell bootstrap when
// terminal.shell_integration is on. Emits ;A (prompt start) / ;B (command start) around the prompt,
// ;C (output start) before each command runs, and ;D;<exit> (command end) at the next prompt - the
// markers TerminalController::HandleAnsiCmd turns into precise command blocks with exit codes. The
// shell is detected from the binary path; bash uses the DEBUG trap (guarded to fire ;C once per
// prompt), zsh uses preexec/precmd hooks. Off by default (it touches the user's PS1).
static std::vector<std::string> BuildShellIntegrationBootstrap(const std::string &shellBinary) {
    bool isZsh = shellBinary.find("zsh") != std::string::npos;
    if (isZsh) {
        return {
            "__goat_osc133_preexec() { printf '\\033]133;C\\007'; }",
            "__goat_osc133_precmd() { printf '\\033]133;D;%d\\007' \"$?\"; }",
            "autoload -Uz add-zsh-hook 2>/dev/null; "
                "add-zsh-hook preexec __goat_osc133_preexec; "
                "add-zsh-hook precmd __goat_osc133_precmd",
            "PS1=$'%{\\033]133;A\\007%}'$PS1$'%{\\033]133;B\\007%}'",
        };
    }
    // bash (and bourne-ish fallbacks): DEBUG trap for ;C, guarded so it fires once per command cycle.
    return {
        "__goat_osc133_done=",
        "__goat_osc133_precmd() { printf '\\033]133;D;%d\\007' \"$?\"; __goat_osc133_done=; }",
        "__goat_osc133_preexec() { [ -n \"$__goat_osc133_done\" ] && return; "
            "__goat_osc133_done=1; printf '\\033]133;C\\007'; }",
        "PROMPT_COMMAND=\"__goat_osc133_precmd${PROMPT_COMMAND:+; $PROMPT_COMMAND}\"",
        "trap '__goat_osc133_preexec' DEBUG",
        "PS1='\\[\\033]133;A\\007\\]'\"$PS1\"'\\[\\033]133;B\\007\\]'",
    };
}

TerminalController::TerminalController() {
    screen.SetHost(this);
}

TerminalController::~TerminalController() {
    // First: the pty reader calls into the screen and this controller - it must be gone before they are
    shell.Close();
    {
        std::lock_guard<std::mutex> guard(syncTimerLock);
        isSyncTimerStopping = true;
    }
    syncTimerWake.notify_one();
    if (syncTimer.joinable()) {
        syncTimer.join();
    }
}

void TerminalController::SetSettings(const ConfigNode &newSettings) {
    settings = newSettings;
    std::lock_guard<std::mutex> lock(screenLock);
    screen.SetScrollbackCap(settings.GetInt("scrollback_lines", TerminalScreen::DEFAULT_SCROLLBACK_LINES));
}

void TerminalController::Begin() {
    logger = gnilk::Logger::GetLogger("TerminalController");
    logger->Debug("Begin");

    RegisterAsOutputConsole();

    inputLine = std::make_shared<Line>();
    inputCursor.position.x = 0;

    shell.SetOutputDelegate([this](const uint8_t *buffer, size_t length) {
        HandleTerminalData(buffer, length);
    });
    shell.SetExitDelegate([this]() {
        OnShellEnded();
    });
    auto altArrows = settings.GetStr("alt_arrows", "");
    if (!altArrows.empty()) {
        isAltArrowsAsWords = (altArrows == "words");
    }

    // Restore persisted scrollback (terminal-scrollback.md §8.2) BEFORE the shell starts - the pty
    // thread mutates the grid the instant shell.Begin() returns, so the seeded history must already be in
    // place. A no-op when nothing was saved, the feature is off, or the .bin is missing/corrupt.
    FromSession(restoreScrollbackFile);
    StartShell();
    LoadCmdHistory();
}

void TerminalController::StartShell() {
    auto shellBinary    = settings.GetStr("shell", "");
    auto shellInitStr   = settings.GetStr("init", "-ils");
    auto shellInitScript = settings.GetSequenceOfStr("bootstrap");
    // §4.2 (TS-3c): opt-in OSC 133 shell integration - appends prompt hooks that emit semantic-prompt
    // markers, giving exact command boundaries + exit codes. Off by default (it rewrites PS1).
    if (settings.GetBool("shell_integration", false)) {
        auto osc133 = BuildShellIntegrationBootstrap(shellBinary);
        shellInitScript.insert(shellInitScript.end(), osc133.begin(), osc133.end());
    }

    if (!shell.Begin(shellBinary, shellInitStr, shellInitScript)) {
        // No pty thread was started, so the state never leaves kIdle - waiting for kRunning would spin
        // forever before the first frame. Carry on without one - the panel says so.
        logger->Error("Shell failed to start - terminal is unavailable");
        OnShellEnded();
        return;
    }

    while (shell.GetState() != Shell::State::kRunning) {
        if (shell.GetState() == Shell::State::kTerminated) {
            logger->Error("Shell got terminated while starting!");
            return;
        }
        std::this_thread::yield();
    }
}

// Pty thread, after the last output. Modes a program left on when it went down with its shell (full screen, mouse
// reports, a hidden cursor, ...) are the panel's again; the notice is on a row of its own.
void TerminalController::OnShellEnded() {
    static constexpr std::string_view resetModes =
        "\x1b[?1l\x1b>\x1b[?1000l\x1b[?1002l\x1b[?1003l\x1b[?1004l\x1b[?1006l\x1b[?2004l\x1b[?2026l"
        "\x1b[4l\x1b[20l\x1b[?6l\x1b[?7h\x1b[?25h\x1b(B\x0f\x1b[r\x1b[0m";
    auto hint = settings.GetBool("enter_restarts_shell", true) ? "Enter starts a new one"
                                                                     : settings.GetStr("shell_ended_hint", "close this tab");
    {
        std::lock_guard<std::mutex> guard(screenLock);
        if (screen.IsAltScreen()) {
            screen.Feed("\x1b[?1049l");
        }
        // The scroll region and origin mode take the cursor home
        auto cursorPos = screen.GetCursorPos();
        screen.Feed(resetModes);
        screen.Feed(fmt::format("\x1b[{};{}H", cursorPos.y + 1, cursorPos.x + 1));
        auto isRowUsed = (cursorPos.x > 0);
        screen.Feed(std::string(isRowUsed ? "\r\n" : "") + "[the shell ended - " + hint + "]\r\n");
        lastOutputAt = std::chrono::steady_clock::now();
        syncStart.reset();
        // A new shell may not mark its output
        useOsc133Boundaries = false;
    }
    isShellEnded = true;
    RequestRedraw();
}

// UI thread. In the same panel: its scrollback, its size, what is typed on the line.
void TerminalController::RestartShell() {
    shell.Close();
    if (doesShellOwnLineEditing) {
        ExitShellOwned();
    }
    isShellEnded = false;
    StartShell();
    std::lock_guard<std::mutex> guard(screenLock);
    shell.SetWindowSize(screen.Cols(), screen.Rows());
}

// A '.goatedit' marker directory in the project registers a kProject search path; its presence opts the
// project into project-local history. Otherwise history is per-user, in the user state dir
// (docs/install-layout.md), read once from a legacy user asset dir when the state file isn't there yet.
void TerminalController::LoadCmdHistory() {
    auto historyFileName = sessionTabId.empty() ? std::string("terminal_history") : "terminal_history." + sessionTabId;
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();

    auto projectPath = assetLoader.ResolveWritePath(historyFileName, AssetLoaderBase::kLocationType::kProject);
    if (!projectPath.empty()) {
        auto histAsset = assetLoader.LoadTextAsset(historyFileName, AssetLoaderBase::kLocationType::kProject);
        cmdHistory.Load(histAsset);
        // Save back to wherever it loaded from
        cmdHistoryPath = (histAsset != nullptr) ? histAsset->GetOriginPath() : projectPath;
        return;
    }

    cmdHistoryPath = InstallPaths::UserStateDir(XDGEnvironment::Instance().GetUserStatePath()) / historyFileName;
    if (!cmdHistory.Load(cmdHistoryPath)) {
        cmdHistory.Load(assetLoader.LoadTextAsset(historyFileName, AssetLoaderBase::kLocationType::kUser));
    }
}

std::filesystem::path TerminalController::ResolveScrollbackBinPath(const std::string &filename) const {
    // Project-scoped: the .bin lives in the same kProject `.goatedit` dir as session.yml. Empty when no
    // project dir is registered - persistence then stands down (gate for the whole feature).
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    return assetLoader.ResolveWritePath(filename, AssetLoaderBase::kLocationType::kProject);
}

void TerminalController::SetSessionTab(const std::string &id, const std::string &scrollbackFile) {
    sessionTabId = id;
    restoreScrollbackFile = scrollbackFile;
}

void TerminalController::ForgetSession() {
    std::error_code ec;
    auto binPath = ResolveScrollbackBinPath(sessionTabId.empty() ? "terminal_scrollback.bin" : "terminal_scrollback." + sessionTabId + ".bin");
    if (!binPath.empty()) {
        std::filesystem::remove(binPath, ec);
    }
    if (!sessionTabId.empty() && !cmdHistoryPath.empty()) {
        std::filesystem::remove(cmdHistoryPath, ec);
    }
}

std::string TerminalController::GetTitle() {
    std::lock_guard<std::mutex> guard(screenLock);
    return title;
}

// On the pty thread, screenLock held
void TerminalController::OnTitle(std::string_view newTitle) {
    if (title == newTitle) {
        return;
    }
    title = std::string(newTitle);
    if (titleChangedHandler) {
        titleChangedHandler();
    }
}

bool TerminalController::ToSession(std::string &outScrollbackFile) {
    outScrollbackFile.clear();

    if (!settings.GetBool("persist_scrollback", true)) {
        return false;
    }
    auto binPath = ResolveScrollbackBinPath(sessionTabId.empty() ? "terminal_scrollback.bin" : "terminal_scrollback." + sessionTabId + ".bin");
    if (binPath.empty()) {
        return false;   // no project .goatedit dir - persistence is project-scoped
    }

    int maxLines = settings.GetInt("persist_scrollback_lines", 2000);
    size_t cap   = (maxLines < 0) ? 0 : (size_t)maxLines;

    // Snapshot under the lock (the pty thread mutates the grid/scrollback); write the file outside it.
    std::vector<Line::Ref> lines;
    {
        std::lock_guard<std::mutex> lock(screenLock);
        lines = screen.SnapshotScrollbackTail(cap);
    }
    if (lines.empty()) {
        return false;   // alt-screen, or nothing has scrolled into history yet
    }

    // Serialise line text + ANSI colour spans to the .bin (lossless, versioned - TS-5a). The Line::Refs
    // are shared into a throwaway buffer purely to reach TextBuffer::SaveWithAttributes.
    TextBuffer buffer;
    for (const auto &line : lines) {
        buffer.AddLine(line);
    }
    if (!buffer.SaveWithAttributes(binPath)) {
        if (logger != nullptr) {
            logger->Error("Failed to write terminal scrollback '%s'", binPath.string().c_str());
        }
        return false;
    }

    outScrollbackFile = binPath.filename().string();
    return true;
}

void TerminalController::FromSession(const std::string &scrollbackFile) {
    if (scrollbackFile.empty()) {
        return;   // nothing persisted (fresh project, or the feature was off when last saved)
    }
    if (!settings.GetBool("persist_scrollback", true)) {
        return;
    }
    auto binPath = ResolveScrollbackBinPath(scrollbackFile);
    if (binPath.empty()) {
        return;   // no project dir - persistence is project-scoped
    }

    TextBuffer buffer;
    if (!buffer.LoadWithAttributes(binPath)) {
        return;   // missing / corrupt / newer version -> start clean (never throws)
    }
    std::vector<Line::Ref> lines;
    lines.reserve(buffer.NumLines());
    for (size_t i = 0; i < buffer.NumLines(); i++) {
        lines.push_back(buffer.LineAt(i));
    }
    if (lines.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(screenLock);
    screen.SeedScrollback(lines);
}

void TerminalController::Resize(int cols, int rows) {
    if (cols == screen.Cols() && rows == screen.Rows()) {
        return;
    }
    std::lock_guard<std::mutex> guard(screenLock);
    ApplyThemeColors();
    screen.Resize(cols, rows);
    shell.SetWindowSize(cols, rows);
}

void TerminalController::RefreshThemeColors() {
    std::lock_guard<std::mutex> guard(screenLock);
    ApplyThemeColors();
}

void TerminalController::ApplyThemeColors() {
    auto &termColors = Editor::Instance().GetTheme()->GetTerminalColor();
    screen.SetDefaultColors(termColors.GetColor("foreground"), termColors.GetColor("background"));
    auto palette = TerminalScreen::DefaultPalette();
    for (int i = 0; i < 16; i++) {
        auto name = std::to_string(i);
        if (termColors.HasColor(name)) {
            palette[i] = termColors.GetColor(name);
        }
    }
    screen.SetPalette(palette);
}

void TerminalController::HandleTerminalData(const uint8_t *buffer, size_t length) {
    bool isHolding = false;
    {
        std::lock_guard<std::mutex> guard(screenLock);
        screen.Feed(buffer, length);
        lastOutputAt = std::chrono::steady_clock::now();

        // While the shell owns the line (completion in progress), mirror what readline
        // has echoed back into our local inputLine so the model stays truthful.
        if (doesShellOwnLineEditing) {
            SyncInputLineFromGrid();
        }

        // Synchronized output: no redraw until the app ends its update - or the cap ends it, with no more output
        if (screen.GetModes().isSynchronizedOutput) {
            if (!syncStart.has_value()) {
                syncStart = std::chrono::steady_clock::now();
                ArmSyncTimer(syncStart.value() + maxSyncHold);
            }
        } else {
            syncStart.reset();
        }
        isHolding = IsHoldingRedraw();
    }
    if (!isHolding) {
        RequestRedraw();
    }
    FeedBuildMessages(buffer, length);
}

// On the pty thread. The summary goes to the status line on the UI thread.
// The command's end (OSC 133;D) was parsed with these bytes, before they get here - the run ends after them. Without
// the marks (shell integration is off by default) a fast build (cc on one file) can start, print and end between two
// reads, so no read sees it run: a moment after the last read the job is looked at once more (OnBuildQuiet).
void TerminalController::FeedBuildMessages(const uint8_t *buffer, size_t length) {
    bool isCommandDone = std::exchange(isCommandDoneSeen, false);
    auto &buildMessages = Editor::Instance().GetBuildMessages();
    if (!buildMessages.IsRunning()) {
        return;
    }
    buildMessages.Feed(std::string_view(reinterpret_cast<const char *>(buffer), length), this);
    bool isEnded = isCommandDone ? buildMessages.OnCommandDone(this) : buildMessages.UpdateJobState(IsForegroundJobRunning(), this);
    if (isEnded) {
        PostBuildSummary();
        return;
    }
    ArmBuildQuietTimer(std::chrono::steady_clock::now() + kBuildQuiet);
}

// On the timer thread, the terminal quiet since the deadline was set
void TerminalController::OnBuildQuiet() {
    if (Editor::Instance().GetBuildMessages().OnQuiet(IsForegroundJobRunning(), this)) {
        PostBuildSummary();
    }
}

void TerminalController::PostBuildSummary() {
    Runloop::PostMessage(0x00, [](uint32_t) {
        auto &messages = Editor::Instance().GetBuildMessages();
        Editor::Instance().ShowStatusMessage(UnicodeHelper::utf8to32("build: " + messages.Summary()));
    });
}

bool TerminalController::IsHoldingRedraw() const {
    if (!screen.GetModes().isSynchronizedOutput || !syncStart.has_value()) {
        return false;
    }
    return (std::chrono::steady_clock::now() - syncStart.value()) < maxSyncHold;
}

void TerminalController::RequestRedraw() {
    if (redrawTrigger) {
        redrawTrigger();
        return;
    }
    Editor::Instance().TriggerUIRedraw();
}

void TerminalController::ArmSyncTimer(std::chrono::steady_clock::time_point deadline) {
    std::lock_guard<std::mutex> guard(syncTimerLock);
    syncTimerDeadline = deadline;
    StartTimerThread();
    syncTimerWake.notify_one();
}

// Each read moves it on: it expires once the terminal has been quiet that long
void TerminalController::ArmBuildQuietTimer(std::chrono::steady_clock::time_point deadline) {
    std::lock_guard<std::mutex> guard(syncTimerLock);
    buildQuietDeadline = deadline;
    StartTimerThread();
    syncTimerWake.notify_one();
}

// Under syncTimerLock
void TerminalController::StartTimerThread() {
    if (!syncTimer.joinable() && !isSyncTimerStopping) {
        syncTimer = std::thread([this]() { SyncTimerLoop(); });
    }
}

// Two deadlines on one thread: the synchronized-output hold (a redraw) and the build run's quiet check
void TerminalController::SyncTimerLoop() {
    std::unique_lock<std::mutex> lock(syncTimerLock);
    while (!isSyncTimerStopping) {
        if (!syncTimerDeadline.has_value() && !buildQuietDeadline.has_value()) {
            syncTimerWake.wait(lock);
            continue;
        }
        auto deadline = !syncTimerDeadline.has_value() ? buildQuietDeadline.value()
                      : !buildQuietDeadline.has_value() ? syncTimerDeadline.value()
                      : std::min(syncTimerDeadline.value(), buildQuietDeadline.value());
        syncTimerWake.wait_until(lock, deadline);
        if (isSyncTimerStopping) {
            break;
        }
        auto now = std::chrono::steady_clock::now();
        bool isSyncDue = syncTimerDeadline.has_value() && (syncTimerDeadline.value() <= now);
        bool isBuildDue = buildQuietDeadline.has_value() && (buildQuietDeadline.value() <= now);
        if (isSyncDue) {
            syncTimerDeadline.reset();
        }
        if (isBuildDue) {
            buildQuietDeadline.reset();
        }
        if (isSyncDue || isBuildDue) {
            lock.unlock();
            if (isSyncDue) {
                RequestRedraw();
            }
            if (isBuildDue) {
                OnBuildQuiet();
            }
            lock.lock();
        }
    }
}

void TerminalController::SyncInputLineFromGrid() {
    // readline owns the line and echoes its edits into the grid. Read the prompt row
    // back from promptAnchorCol (the prompt end) up to the trailing content and mirror
    // it into inputLine. Caret column is the grid cursor relative to the prompt end.
    auto cursorPos = screen.GetCursorPos();
    // The prompt's row by its abs id: when the line wraps on the bottom row the screen scrolls and the prompt moves
    // up a grid row - it is still the prompt's row once the line is erased back onto it.
    const auto &row = screen.GetRow(cursorPos.y);
    if (screen.AbsRowCount() != promptAnchorAbs) {
        if (!IsPromptRow(row)) {
            // The cursor is off the prompt's row (the line wrapped, a question below it). Leave the
            // mirror as-is, but it no longer holds the whole line.
            isLineMirrorValid = false;
            return;
        }
        // The shell redrew the prompt here - under a list of completion candidates (Tab Tab)
        promptAnchorAbs = screen.AbsRowCount();
    }

    int start = std::clamp(promptAnchorCol, 0, (int)row.size());
    int end   = (int)row.size();
    // Trim trailing blanks so we don't carry the rest of the (padded) grid row.
    while (end > start && row[end - 1].ch == U' ') {
        end--;
    }
    // A line that reaches the last column may go on on the next row - we only read this one
    isLineMirrorValid = (end < (int)row.size());

    std::u32string lineText;
    for (int x = start; x < end; x++) {
        if (row[x].width != 0) {
            lineText += row[x].ch;
        }
    }

    inputLine->Clear();
    inputLine->Append(lineText);
    inputCursor.position.x = std::clamp(cursorPos.x - promptAnchorCol, 0, (int)inputLine->Length());
}

//
// gnilk::vt::ITerminalHost - inside HandleTerminalData, screenLock held
//
// Everything the terminal sends - replies, keys, pastes, mouse, focus - goes out here
void TerminalController::WriteToPty(std::string_view bytes) {
    if (ptyWriter) {
        ptyWriter(bytes);
        return;
    }
    shell.WriteBytes(std::string(bytes));
}

// OSC 133 (§4.2). The first marker switches the CommitLine heuristic off (useOsc133Boundaries): from then on the
// shell marks where a command's output starts (C), exactly.
void TerminalController::OnSemanticPrompt(char kind, std::string_view params) {
    useOsc133Boundaries = true;
    if (kind == 'C') {
        screen.MarkOutputStart(true);
    } else if (kind == 'D') {
        // The build messages hear it once the bytes it came with are fed to them (FeedBuildMessages)
        isCommandDoneSeen = true;
    }
}

bool TerminalController::HandleKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress) {
    // Three handling paths, checked in priority order. The first two both FORWARD the key to the
    // pty (via ForwardKeyPressToShell) but for different, independent reasons; the third edits
    // locally. See the doesShellOwnLineEditing comment in the header for why these are two separate
    // dimensions.

    // (1) An application owns the terminal (alternate screen, or a foreground job). This wins over
    //     everything else - while it runs, every key is its input.
    if (IsAppActive()) {
        return ForwardKeyPressToShell(keyPress);
    }

    // (2) readline owns the line (entered on Tab/ShellCompletion). Keys pass through to the pty so
    //     readline can drive completion. Ctrl+C aborts readline's line, so we must drop back to
    //     local editing here or we'd be stuck in passthrough with a stale prompt anchor.
    if (DoesShellOwnLineEditing()) {
        bool handled = ForwardKeyPressToShell(keyPress);
        if (keyPress.IsCtrlPressed() && (keyPress.key == 'c' || keyPress.key == 'C')) {
            ExitShellOwned();
        }
        return handled;
    }

    // (3) Default (local editing): we own the line. Edit inputLine locally; nothing reaches the shell
    //     until the line is committed.
    if (IsInterruptKey(keyPress)) {
        InterruptLocalLine();
        cursor.position.x = GetCursorXPos();
        return true;
    }
    // A Ctrl chord nothing is bound to is not text (the ANSI backend delivers Ctrl+letter as the letter) - it is the
    // shell's (Ctrl+W, Ctrl+T, Ctrl+K, ...)
    if (keyPress.IsCtrlPressed() && !keyPress.isSpecialKey) {
        return HandCtrlKeyToShell(keyPress);
    }
    if (DefaultEditLine(inputCursor, inputLine, keyPress)) {
        cursor.position.x = GetCursorXPos();
        ScrollToBottom();   // §5.3: any local text input snaps the viewport back to the bottom
        return true;
    }
    return false;
}

namespace {

    uint8_t ToVtModifiers(const KeyPress &keyPress) {
        uint8_t modifiers = gnilk::vt::kModNone;
        if (keyPress.IsShiftPressed()) {
            modifiers |= gnilk::vt::kModShift;
        }
        if (keyPress.IsAltPressed()) {
            modifiers |= gnilk::vt::kModAlt;
        }
        if (keyPress.IsCtrlPressed()) {
            modifiers |= gnilk::vt::kModCtrl;
        }
        return modifiers;
    }

    std::optional<gnilk::vt::Key> ToVtKey(int specialKey) {
        using gnilk::vt::Key;
        switch (specialKey) {
            case Keyboard::kKeyCode_Return:         return Key::kEnter;
            case Keyboard::kKeyCode_Tab:            return Key::kTab;
            case Keyboard::kKeyCode_Backspace:      return Key::kBackspace;
            case Keyboard::kKeyCode_Escape:         return Key::kEscape;
            case Keyboard::kKeyCode_UpArrow:        return Key::kUp;
            case Keyboard::kKeyCode_DownArrow:      return Key::kDown;
            case Keyboard::kKeyCode_RightArrow:     return Key::kRight;
            case Keyboard::kKeyCode_LeftArrow:      return Key::kLeft;
            case Keyboard::kKeyCode_Home:           return Key::kHome;
            case Keyboard::kKeyCode_End:            return Key::kEnd;
            case Keyboard::kKeyCode_PageUp:         return Key::kPageUp;
            case Keyboard::kKeyCode_PageDown:       return Key::kPageDown;
            case Keyboard::kKeyCode_Insert:         return Key::kInsert;
            case Keyboard::kKeyCode_DeleteForward:  return Key::kDelete;
            case Keyboard::kKeyCode_F1:             return Key::kF1;
            case Keyboard::kKeyCode_F2:             return Key::kF2;
            case Keyboard::kKeyCode_F3:             return Key::kF3;
            case Keyboard::kKeyCode_F4:             return Key::kF4;
            case Keyboard::kKeyCode_F5:             return Key::kF5;
            case Keyboard::kKeyCode_F6:             return Key::kF6;
            case Keyboard::kKeyCode_F7:             return Key::kF7;
            case Keyboard::kKeyCode_F8:             return Key::kF8;
            case Keyboard::kKeyCode_F9:             return Key::kF9;
            case Keyboard::kKeyCode_F10:            return Key::kF10;
            case Keyboard::kKeyCode_F11:            return Key::kF11;
            case Keyboard::kKeyCode_F12:            return Key::kF12;
            default:                                return std::nullopt;
        }
    }

}

bool TerminalController::ForwardKeyPressToShell(const KeyPress &keyPress) {
    // Cmd (macOS) is the editor's - Save, Quit, ... keep working while an app runs
    if (keyPress.IsCommandPressed()) {
        return false;
    }
    if (IsEndOfInputKey(keyPress) && IsShellOwnedLineEmpty()) {
        return true;
    }
    auto modifiers = ToVtModifiers(keyPress);
    if (keyPress.IsHumanReadable()) {
        WriteToPty(gnilk::vt::EncodeText(keyPress.key, modifiers));
        return true;
    }
    if (keyPress.isSpecialKey) {
        auto key = ToVtKey(keyPress.specialKey);
        if (!key.has_value()) {
            return false;
        }
        bool isWordArrow = (*key == gnilk::vt::Key::kLeft) || (*key == gnilk::vt::Key::kRight);
        if (isAltArrowsAsWords && isWordArrow && (modifiers == gnilk::vt::kModAlt)) {
            WriteToPty((*key == gnilk::vt::Key::kLeft) ? "\x1b" "b" : "\x1b" "f");
            return true;
        }
        WriteToPty(gnilk::vt::EncodeKey(*key, modifiers, GetModes()));
        return true;
    }
    // A backend that already delivers the control character
    if (keyPress.isKeyValid && (keyPress.key != 0) && (keyPress.key < 0x80)) {
        WriteToPty(std::string(1, static_cast<char>(keyPress.key)));
        return true;
    }
    return false;
}

// Ctrl+D; from a backend that delivers the control character too
bool TerminalController::IsEndOfInputKey(const KeyPress &keyPress) {
    if (keyPress.isSpecialKey) {
        return false;
    }
    bool isCtrlD = keyPress.IsCtrlPressed() && !keyPress.IsShiftPressed() && !keyPress.IsAltPressed() &&
                   ((keyPress.key == 'd') || (keyPress.key == 'D'));
    return isCtrlD || (keyPress.key == 0x04);
}

// readline has the line (a Ctrl chord or Tab handed it over) and nothing is on it: Ctrl+D would end the shell and leave
// the panel dead. A program reading its input is not this - it gets its end of file.
bool TerminalController::IsShellOwnedLineEmpty() {
    if (!doesShellOwnLineEditing || IsAppActive()) {
        return false;
    }
    std::lock_guard<std::mutex> guard(screenLock);
    return inputLine->Length() == 0;
}

// Passthrough: the action's own key goes to the pty, encoded - Esc, Ctrl+C, Shift+Tab, Ctrl+arrows reach the app
// even though the editor binds them. Escape-hatch actions return false so the view hands them to the editor.
bool TerminalController::ForwardActionToShell(const EditorAction &kpAction) {
    if (IsTerminalEscapeHatch(kpAction)) {
        return false;
    }
    if ((kpAction.uiAction == kUIAction::kUIActionCommitLine) && DoesShellOwnLineEditing()) {
        CommitShellOwnedLine();
        return true;
    }
    bool isHandled = ForwardKeyPressToShell(kpAction.keyPress);
    // Ctrl+C aborts readline's line: back to local editing, or we'd be stuck in passthrough with a stale anchor
    auto &key = kpAction.keyPress;
    if (doesShellOwnLineEditing && key.IsCtrlPressed() && ((key.key == 'c') || (key.key == 'C'))) {
        ExitShellOwned();
    }
    return isHandled;
}

// Enter while readline owns the line
void TerminalController::CommitShellOwnedLine() {
    // Bug #17: a dot-command finished in readline is the editor's, not the shell's
    if (CommitShellOwnedDotCommand()) {
        return;
    }
    // §4.1 tab-completion edge case: this commit never goes through CommitLine(). readline's edits have been
    // mirrored into inputLine on the pty thread; mirror once more from the grid as it stands now and read it
    // under the lock, before ExitShellOwned() clears it. Best effort: an echo still in flight is not seen.
    std::u32string cmdLine;
    {
        std::lock_guard<std::mutex> guard(screenLock);
        SyncInputLineFromGrid();
        cmdLine = inputLine->Buffer();
    }
    RecordCommand(cmdLine);
    if (!cmdLine.empty()) {
        OnShellCommand(cmdLine);
    }
    WriteToPty("\r");
    ExitShellOwned();
}

bool TerminalController::IsTerminalEscapeHatch(const EditorAction &kpAction) {
    if (kpAction.keyPress.IsCommandPressed()) {
        return true;
    }
    switch (kpAction.uiAction) {
        case kUIAction::kUIActionCycleActiveView:
        case kUIAction::kUIActionCycleActiveViewNext:
        case kUIAction::kUIActionCycleActiveViewPrev:
        case kUIAction::kUIActionIncreaseViewWidth:
        case kUIAction::kUIActionDecreaseViewWidth:
        case kUIAction::kUIActionIncreaseViewHeight:
        case kUIAction::kUIActionDecreaseViewHeight:
        case kUIAction::kUIActionMaximizeViewHeight:
            return true;
        default:
            break;
    }
    switch (kpAction.action) {
        // Buffer switching (Alt+Left/Right, Alt+1..9) is not here: an app gets those keys - readline's word
        // motion, Alt+N in tmux/irssi. Cmd+N (macOS) still switches; or Alt+E to the editor first.
        case kAction::kActionCycleActiveEditor:
        case kAction::kActionSwitchToTerminal:
        case kAction::kActionSwitchToEditor:
        case kAction::kActionSwitchToProject:
        case kAction::kActionViewModeText:
        case kAction::kActionViewModeHex:
        case kAction::kActionPasteFromClipboard:
            return true;
        default:
            return false;
    }
}

bool TerminalController::IsAppActive() {
    {
        std::lock_guard<std::mutex> guard(screenLock);
        if (screen.IsAltScreen()) {
            return true;
        }
    }
    return IsForegroundJobRunning();
}

bool TerminalController::IsForegroundJobRunning() {
    return foregroundJobProbe ? foregroundJobProbe() : shell.IsForegroundJobRunning();
}

gnilk::vt::Modes TerminalController::GetModes() {
    std::lock_guard<std::mutex> guard(screenLock);
    return screen.GetModes();
}

void TerminalController::Paste(const std::u32string &text) {
    if (text.empty()) {
        return;
    }
    auto bytes = UnicodeHelper::utf32to8(text);
    if (IsAppActive() || DoesShellOwnLineEditing()) {
        WriteToPty(gnilk::vt::EncodePaste(bytes, GetModes()));
        return;
    }
    if (text.find_first_of(U"\r\n") == std::u32string::npos) {
        inputCursor.position.x = std::clamp(inputCursor.position.x, 0, (int)inputLine->Length());
        inputLine->Insert(inputCursor.position.x, text);
        inputCursor.position.x += (int)text.length();
        ScrollToBottom();
        return;
    }
    if (isShellEnded) {
        return;
    }
    // Several lines: readline takes the line (as on Tab completion). Bracketed when the shell asked for it (bash 5.1+,
    // zsh): the paste lands in the line, visible and editable, run on Enter. A shell that didn't (macOS' bash 3.2)
    // gets the lines as typed - each runs at its newline, as any terminal pastes them; brackets it doesn't know
    // showed up as '00~' / '01~' in its line
    EnterShellOwned();
    WriteToPty(UnicodeHelper::utf32to8(inputLine->Buffer()) + gnilk::vt::EncodePaste(bytes, GetModes()));
    ScrollToBottom();
}

void TerminalController::Promote(std::u32string text, bool isSubmit) {
    while (!text.empty() && ((text.back() == U'\n') || (text.back() == U'\r'))) {
        text.pop_back();
    }
    if (text.empty()) {
        return;
    }
    // Paste may hand the line to readline (several lines at the local prompt) - so ask where Enter goes after it
    Paste(text);
    if (!isSubmit) {
        return;
    }
    if (IsAppActive()) {
        WriteToPty("\r");
    } else if (DoesShellOwnLineEditing()) {
        CommitShellOwnedLine();
    } else {
        CommitLine();
    }
}

void TerminalController::SetFocus(bool isFocused) {
    hasFocus = isFocused;
    auto bytes = gnilk::vt::EncodeFocus(isFocused, GetModes());
    if (!bytes.empty()) {
        WriteToPty(bytes);
    }
}

bool TerminalController::IsSettled(std::chrono::milliseconds quietTime) {
    std::lock_guard<std::mutex> guard(screenLock);
    if (!screen.GetModes().isBracketedPaste) {
        return false;
    }
    return (std::chrono::steady_clock::now() - lastOutputAt) >= quietTime;
}

std::chrono::steady_clock::time_point TerminalController::LastOutputAt() {
    std::lock_guard<std::mutex> guard(screenLock);
    return lastOutputAt;
}

bool TerminalController::ForwardMouse(gnilk::vt::MouseButton button, gnilk::vt::MouseAction action, int x, int y,
                                      uint8_t modifiers) {
    auto bytes = gnilk::vt::EncodeMouse(button, action, x, y, modifiers, GetModes());
    if (bytes.empty()) {
        return false;
    }
    WriteToPty(bytes);
    return true;
}

bool TerminalController::IsInterruptKey(const KeyPress &keyPress) {
    return keyPress.IsCtrlPressed() && !keyPress.IsShiftPressed() && !keyPress.IsAltPressed() &&
           !keyPress.isSpecialKey && ((keyPress.key == 'c') || (keyPress.key == 'C'));
}

// The line was only ours (local edits never reach the pty), so the shell gets just the interrupt: ^C and a fresh
// prompt on the next row, as in any terminal. Not run, not in history. Sending the text first would not show it - the
// tty's SIGINT flushes it unread - so we echo it into the grid ourselves (the grid cursor is parked at the prompt
// end); bash prints the ^C after it, zsh prints none.
void TerminalController::InterruptLocalLine() {
    std::u32string echo;
    for (auto ch : inputLine->Buffer()) {
        if (ch >= U' ') {
            echo += ch;
        }
    }
    if (!echo.empty()) {
        auto bytes = UnicodeHelper::utf32to8(echo);
        std::lock_guard<std::mutex> guard(screenLock);
        screen.Feed(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
    }
    WriteToPty("\x03");
    inputLine->Clear();
    inputCursor.position.x = 0;
    cmdHistory.ResetNavigation();
    ScrollToBottom();
}

bool TerminalController::OnAction(const EditorAction &kpAction) {
    // Ctrl+C is CopyToClipboard on Linux - at the prompt it is the interrupt key, as in any terminal
    if (IsInterruptKey(kpAction.keyPress)) {
        InterruptLocalLine();
        Editor::Instance().TriggerUIRedraw();
        return true;
    }
    if ((kpAction.action == kAction::kActionShellCompletion) && isShellEnded) {
        return true;
    }
    if (kpAction.action == kAction::kActionShellCompletion) {
        // First Tab in local-edit mode: hand the line over to readline so it can
        // complete. The real grid cursor has stayed parked at the prompt end the
        // whole time we edited locally (local edits never touch the grid), so
        // capture it now as the prompt anchor for reading the line back. From here
        // readline owns the line until the command is committed or aborted.
        EnterShellOwned();
        auto cmdLine = std::u32string(inputLine->Buffer());
        WriteToPty(UnicodeHelper::utf32to8(cmdLine) + "\t");
        return true;
    }

    switch (kpAction.uiAction) {
        case kUIAction::kUIActionLineHome :
            inputCursor.position.x = 0;
            break;
        case kUIAction::kUIActionLineEnd :
            inputCursor.position.x = inputLine->Length();
            break;
        case kUIAction::kUIActionLineLeft :
            inputCursor.position.x = std::max(0, inputCursor.position.x - 1);
            break;
        case kUIAction::kUIActionLineRight :
            inputCursor.position.x = std::min((int)inputLine->Length(), inputCursor.position.x + 1);
            break;
        case kUIAction::kUIActionLineUp : {
            auto entry = cmdHistory.NavigateUp();
            if (entry.has_value()) {
                inputLine->Clear();
                inputLine->Append(entry.value());
                inputCursor.position.x = (int)inputLine->Length();
            }
            break;
        }
        case kUIAction::kUIActionLineDown : {
            auto entry = cmdHistory.NavigateDown();
            inputLine->Clear();
            if (entry.has_value()) {
                inputLine->Append(entry.value());
            }
            inputCursor.position.x = (int)inputLine->Length();
            break;
        }
        // §5.3: at the prompt (local edit), Page/BufferStart/End page the BACKLOG, not the cursor —
        // the prompt is a single line, so there's nothing else for these to mean here.
        case kUIAction::kUIActionPageUp :
            ScrollViewport(-(int64_t)std::max(1, screen.Rows() - 1));
            break;
        case kUIAction::kUIActionPageDown :
            ScrollViewport((int64_t)std::max(1, screen.Rows() - 1));
            break;
        case kUIAction::kUIActionBufferStart :
            ScrollToTop();
            break;
        case kUIAction::kUIActionBufferEnd :
            ScrollToBottom();
            break;
        default:
            return false;
    }
    Editor::Instance().TriggerUIRedraw();
    return true;
}

void TerminalController::ScrollToBottom() {
    std::lock_guard<std::mutex> guard(screenLock);
    followBottom = true;
    anchorAbsRow = screen.AbsRowCount();
}

void TerminalController::ScrollToTop() {
    std::lock_guard<std::mutex> guard(screenLock);
    followBottom = false;
    anchorAbsRow = screen.ScrollbackBase();
}

uint64_t TerminalController::CurrentTopVisibleAbsRow() const {
    if (!followBottom) {
        return anchorAbsRow;
    }
    // Must mirror TerminalView::DrawViewContents' followBottom windowTop exactly: that branch
    // already shows rows [bottom-visibleRows, bottom), so anything measuring "from where we are"
    // has to start from THAT window's top, not from 'bottom' itself - using 'bottom' directly is one
    // page short (see the TS-0f first-PageUp flicker this fixed).
    uint64_t bottom = screen.AbsRowCount();
    uint64_t top    = screen.ScrollbackBase();
    int64_t historyRows = (int64_t)(bottom - top);
    int64_t visibleRows = std::max(0, screen.Rows() - 1);
    return bottom - (uint64_t)std::min(historyRows, visibleRows);
}

void TerminalController::ScrollViewport(int64_t deltaRows) {
    std::lock_guard<std::mutex> guard(screenLock);
    uint64_t bottom = screen.AbsRowCount();
    uint64_t top    = screen.ScrollbackBase();
    uint64_t current = CurrentTopVisibleAbsRow();

    // The newest window is the followBottom one: scrolling down stops there, it never shows a part-empty page
    int64_t historyRows = (int64_t)(bottom - top);
    int64_t visibleRows = std::max(0, screen.Rows() - 1);
    int64_t newestTop = (int64_t)bottom - std::min(historyRows, visibleRows);

    int64_t next = (int64_t)current + deltaRows;
    next = std::clamp(next, (int64_t)top, newestTop);

    anchorAbsRow = (uint64_t)next;
    followBottom = (next >= newestTop);
    if (followBottom) {
        anchorAbsRow = bottom;
    }
}

std::optional<std::u32string> TerminalController::GetLastOutputText() {
    std::lock_guard<std::mutex> guard(screenLock);
    auto rows = screen.GetLastOutputText();
    if (rows.empty()) {
        return std::nullopt;
    }
    std::u32string text;
    for (size_t i = 0; i < rows.size(); i++) {
        if (i > 0) {
            text += U'\n';
        }
        text += rows[i];
    }
    return text;
}

// An unbound Ctrl chord at the local prompt acts on the line in the shell, as in any terminal: readline takes the line
// (as on Tab), its cursor goes where the caret is, then the key. Not Ctrl+D - on an empty line it ends the shell.
bool TerminalController::HandCtrlKeyToShell(const KeyPress &keyPress) {
    if (keyPress.IsCommandPressed() || (keyPress.key == 'd') || (keyPress.key == 'D') || isShellEnded) {
        return false;
    }
    EnterShellOwned();
    auto cmdLine = std::u32string(inputLine->Buffer());
    std::string bytes = UnicodeHelper::utf32to8(cmdLine);
    auto caret = std::clamp(inputCursor.position.x, 0, (int)cmdLine.size());
    for (size_t i = (size_t)caret; i < cmdLine.size(); i++) {
        bytes += gnilk::vt::EncodeKey(gnilk::vt::Key::kLeft, gnilk::vt::kModNone, GetModes());
    }
    WriteToPty(bytes);
    ForwardKeyPressToShell(keyPress);
    return true;
}

// readline takes the line. Until its echo arrives, inputLine (what was typed) is the line.
void TerminalController::EnterShellOwned() {
    std::lock_guard<std::mutex> guard(screenLock);
    auto cursorPos = screen.GetCursorPos();
    promptAnchorCol = cursorPos.x;
    promptAnchorAbs = screen.AbsRowCount();
    promptText = PromptTextOf(screen.GetRow(cursorPos.y));
    isLineMirrorValid = true;
    doesShellOwnLineEditing = true;
}

void TerminalController::ExitShellOwned() {
    // Return to local line editing. The committed/aborted line lives in the shell's
    // output now; our local buffer starts fresh at the next prompt. Under the lock: the
    // pty thread mirrors into inputLine while the flag is set.
    std::lock_guard<std::mutex> guard(screenLock);
    doesShellOwnLineEditing = false;
    isLineMirrorValid = false;
    inputLine->Clear();
    inputCursor.position.x = 0;
}

// The prompt: the row's text left of the anchor column (wide characters' right halves skipped)
std::u32string TerminalController::PromptTextOf(const TerminalScreen::Row &row) const {
    std::u32string text;
    int end = std::min(promptAnchorCol, (int)row.size());
    for (int x = 0; x < end; x++) {
        if (row[x].width != 0) {
            text += row[x].ch;
        }
    }
    return text;
}

// The row starts with the prompt the line was anchored to - the shell redrew it there. An empty prompt never matches
// (every row would). A prompt that changed when redrawn (a clock) doesn't either: the line can't be read back then.
bool TerminalController::IsPromptRow(const TerminalScreen::Row &row) const {
    if (promptText.empty() || ((int)row.size() < promptAnchorCol)) {
        return false;
    }
    return PromptTextOf(row) == promptText;
}

// Enter on a line readline owns (after Tab) that is a dot-command: clear readline's line - end of line, kill line
// (bash and zsh, emacs mode) - and give it an empty line for a fresh prompt, then run the command as CommitLine does.
// The command is the mirror up to the cursor: what the shell draws after it (zsh's RPROMPT, an autosuggestion) is
// not typed text. Only from a mirror that holds the whole line - otherwise false, and the shell gets its own line.
bool TerminalController::CommitShellOwnedDotCommand() {
    std::u32string cmdLine;
    {
        std::lock_guard<std::mutex> guard(screenLock);
        SyncInputLineFromGrid();
        if (!isLineMirrorValid) {
            return false;
        }
        cmdLine = inputLine->Buffer().substr(0, inputCursor.position.x);
    }
    if (!PluginExecutor::IsPluginCommandLine(cmdLine)) {
        return false;
    }
    {
        std::lock_guard<std::mutex> guard(screenLock);
        screen.MarkOutputEnd();
    }
    WriteToPty("\x05\x15\r");
    ExitShellOwned();
    RecordCommand(cmdLine);
    PluginExecutor::ParseAndExecuteWithCmdPrefix(cmdLine);
    ScrollToBottom();
    Editor::Instance().TriggerUIRedraw();
    return true;
}

int TerminalController::GetCursorXPos() {
    return screen.GetCursorPos().x + inputCursor.position.x;
}

void TerminalController::CommitLine() {
    // What readline had when its shell ended ('exit') is not a line of ours
    if (isShellEnded && doesShellOwnLineEditing) {
        ExitShellOwned();
    }
    std::u32string cmdLine(inputLine->Buffer());
    if (isShellEnded && !PluginExecutor::IsPluginCommandLine(cmdLine)) {
        // What is typed stays - the next Enter runs it
        if (settings.GetBool("enter_restarts_shell", true)) {
            RestartShell();
            ScrollToBottom();
            Editor::Instance().TriggerUIRedraw();
        }
        return;
    }
    RecordCommand(cmdLine);

    inputLine->Clear();
    inputCursor.position.x = 0;

    // A dot-command runs in the editor - it is no shell command, and leaves the last output alone (`.co` copies
    // the output of the command before it). It reads like one: the command on its prompt line, the output below,
    // then the shell's next prompt.
    if (PluginExecutor::IsPluginCommandLine(cmdLine)) {
        RunDotCommand(cmdLine);
    } else {
        if (!cmdLine.empty()) {
            OnShellCommand(cmdLine);
        }
        cmdLine += U"\n";
        shell.SendCmd(cmdLine);
    }

    ScrollToBottom();   // §5.3: committing a line snaps the viewport back to the bottom
    Editor::Instance().TriggerUIRedraw();
}

// The command goes on the prompt line, where the shell would have echoed it, and the cursor to the row below - so
// the output (WriteLine) lands under it, not above the prompt. The shell still thinks its cursor is at the end of
// that prompt: after the output the cursor goes up onto the last row written, and the empty line sent to the shell
// moves down off it and draws the next prompt right below. The shell is idle at its prompt - nothing else writes.
void TerminalController::RunDotCommand(const std::u32string &cmdLine) {
    {
        std::lock_guard<std::mutex> guard(screenLock);
        screen.MarkOutputEnd();
        screen.Feed(ConsoleText(cmdLine) + "\r\n");
    }
    PluginExecutor::ParseAndExecuteWithCmdPrefix(cmdLine);
    if (isShellEnded) {
        return;     // no prompt to bring back
    }
    {
        std::lock_guard<std::mutex> guard(screenLock);
        screen.Feed("\x1b[A");
    }
    static auto newline = std::u32string(U"\n");
    shell.SendCmd(newline);
}

// A committed command line - typed locally (CommitLine) or finished in readline after Tab (ForwardActionToShell):
// into the history, saved
void TerminalController::RecordCommand(const std::u32string &cmdLine) {
    if (!cmdLine.empty()) {
        cmdHistory.Push(cmdLine);
        if (!cmdHistoryPath.empty()) {
            cmdHistory.Save(cmdHistoryPath);
        }
    }
    cmdHistory.ResetNavigation();
}

// A command went to the shell: its output starts below the line it was typed on - unless the shell says where
// (OSC 133;C). Call without screenLock held.
void TerminalController::OnShellCommand(const std::u32string &cmdLine) {
    MarkCommandOutput();
    auto &editor = Editor::Instance();
    auto &roots = editor.GetIndexedRoots();
    auto projectRoot = roots.empty() ? std::string{} : roots.front().string();
    editor.GetBuildMessages().OnCommand(UnicodeHelper::utf32to8(cmdLine), shell.GetWorkingDirectory(), projectRoot, this);
}

void TerminalController::MarkCommandOutput() {
    std::lock_guard<std::mutex> guard(screenLock);
    if (!useOsc133Boundaries) {
        screen.MarkOutputStart(false);
    }
}

// Console output from the editor (JS Console, plugins) - printed as text above the shell's line. '\n' breaks the line;
// other control characters except TAB show as U+FFFD, so the text can't drive the terminal.
void TerminalController::WriteLine(const std::u32string &str) {
    {
        std::lock_guard<std::mutex> guard(screenLock);
        // Above the shell's prompt, which may have arrived already (hitlist #51: the start-up banner pushed bash's
        // prompt off the panel). Under a dot-command typed at the prompt the cursor is on an empty row below it
        // (RunDotCommand) - the same call then just prints the line there.
        screen.WriteAboveCursorLine(ConsoleText(str));
    }
    Editor::Instance().TriggerUIRedraw();
}

// Text from the editor as terminal bytes: '\n' breaks the line, other control characters except TAB show as U+FFFD
std::string TerminalController::ConsoleText(const std::u32string &str) {
    std::u32string text;
    text.reserve(str.size());
    for (auto ch : str) {
        if (ch == U'\n') {
            text += U"\r\n";
            continue;
        }
        bool isControl = (ch < 0x20) || (ch == 0x7F) || ((ch >= 0x80) && (ch < 0xA0));
        text.push_back((isControl && (ch != U'\t')) ? U'\uFFFD' : ch);
    }
    return UnicodeHelper::utf32to8(text);
}

//
// Created by gnilk on 22.02.2024.
//

#include <algorithm>

#include "TerminalController.h"
#include "Core/Editor.h"
#include "Core/HexDump.h"
#include "Core/Plugins/PluginExecutor.h"
#include "Core/UnicodeHelper.h"
#include "Core/Session/SessionManager.h"
#include "Core/Session/SessionState.h"
#include "Core/InstallPaths.h"
#include "Core/XDGEnvironment.h"

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
    {
        std::lock_guard<std::mutex> guard(syncTimerLock);
        isSyncTimerStopping = true;
    }
    syncTimerWake.notify_one();
    if (syncTimer.joinable()) {
        syncTimer.join();
    }
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

    auto shellBinary    = Config::Instance()["terminal"].GetStr("shell", "");
    auto shellInitStr   = Config::Instance()["terminal"].GetStr("init", "-ils");
    auto shellInitScript = Config::Instance()["terminal"].GetSequenceOfStr("bootstrap");
    auto altArrows = Config::Instance()["terminal"].GetStr("alt_arrows", "");
    if (!altArrows.empty()) {
        isAltArrowsAsWords = (altArrows == "words");
    }
    // §4.2 (TS-3c): opt-in OSC 133 shell integration - appends prompt hooks that emit semantic-prompt
    // markers, giving exact command boundaries + exit codes. Off by default (it rewrites PS1).
    if (Config::Instance()["terminal"].GetBool("shell_integration", false)) {
        auto osc133 = BuildShellIntegrationBootstrap(shellBinary);
        shellInitScript.insert(shellInitScript.end(), osc133.begin(), osc133.end());
    }

    // Restore persisted scrollback (terminal-scrollback.md §8.2) BEFORE the shell starts - the pty
    // thread mutates the grid the instant shell.Begin() returns, so the seeded history must already be in
    // place. A no-op when nothing was saved, the feature is off, or the .bin is missing/corrupt.
    FromSession(SessionManager::Instance().CurrentSession().terminal);

    if (!shell.Begin(shellBinary, shellInitStr, shellInitScript)) {
        // No pty thread was started, so the state never leaves kIdle - waiting for kRunning would spin
        // forever before the first frame. Carry on with a dead terminal instead.
        logger->Error("Shell failed to start - terminal is unavailable");
        return;
    }

    while (shell.GetState() != Shell::State::kRunning) {
        if (shell.GetState() == Shell::State::kTerminated) {
            logger->Error("Shell got terminated while starting!");
            return;
        }
        std::this_thread::yield();
    }

    LoadCmdHistory();
}

// A '.goatedit' marker directory in the project registers a kProject search path; its presence opts the
// project into project-local history. Otherwise history is per-user, in the user state dir
// (docs/install-layout.md), read once from a legacy user asset dir when the state file isn't there yet.
void TerminalController::LoadCmdHistory() {
    static const char *historyFileName = "terminal_history";
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

bool TerminalController::ToSession(TerminalSession &out) {
    out = TerminalSession{};   // fresh - overwrite any state lingering from the restore-time load

    if (!Config::Instance()["terminal"].GetBool("persist_scrollback", true)) {
        return false;
    }
    auto binPath = ResolveScrollbackBinPath("terminal_scrollback.bin");
    if (binPath.empty()) {
        return false;   // no project .goatedit dir - persistence is project-scoped
    }

    int maxLines = Config::Instance()["terminal"].GetInt("persist_scrollback_lines", 2000);
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

    out.scrollbackFile = binPath.filename().string();
    return true;
}

void TerminalController::FromSession(const TerminalSession &in) {
    if (in.scrollbackFile.empty()) {
        return;   // nothing persisted (fresh project, or the feature was off when last saved)
    }
    if (!Config::Instance()["terminal"].GetBool("persist_scrollback", true)) {
        return;
    }
    auto binPath = ResolveScrollbackBinPath(in.scrollbackFile);
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
    if (!syncTimer.joinable()) {
        syncTimer = std::thread([this]() { SyncTimerLoop(); });
    }
    syncTimerWake.notify_one();
}

void TerminalController::SyncTimerLoop() {
    std::unique_lock<std::mutex> lock(syncTimerLock);
    while (!isSyncTimerStopping) {
        if (!syncTimerDeadline.has_value()) {
            syncTimerWake.wait(lock);
            continue;
        }
        auto deadline = syncTimerDeadline.value();
        bool isExpired = syncTimerWake.wait_until(lock, deadline) == std::cv_status::timeout;
        if (isExpired && (syncTimerDeadline == deadline) && !isSyncTimerStopping) {
            syncTimerDeadline.reset();
            lock.unlock();
            RequestRedraw();
            lock.lock();
        }
    }
}

void TerminalController::SyncInputLineFromGrid() {
    // readline owns the line and echoes its edits into the grid. Read the prompt row
    // back from promptAnchor.x (the prompt end) up to the trailing content and mirror
    // it into inputLine. Caret column is the grid cursor relative to the prompt end.
    auto cursorPos = screen.GetCursorPos();
    if (cursorPos.y != promptAnchor.y) {
        // The prompt moved (the line wrapped, or a candidate list redrew it lower).
        // Re-anchoring is the ambiguous-completion case — deferred. Leave the mirror
        // as-is, but it no longer holds the whole line.
        isLineMirrorValid = false;
        return;
    }

    const auto &row = screen.GetRow(cursorPos.y);
    int start = std::clamp(promptAnchor.x, 0, (int)row.size());
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
    inputCursor.position.x = std::clamp(cursorPos.x - promptAnchor.x, 0, (int)inputLine->Length());
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
    if (doesShellOwnLineEditing) {
        bool handled = ForwardKeyPressToShell(keyPress);
        if (keyPress.IsCtrlPressed() && (keyPress.key == 'c' || keyPress.key == 'C')) {
            ExitShellOwned();
        }
        return handled;
    }

    // (3) Default (local editing): we own the line. Edit inputLine locally; nothing reaches the shell
    //     until the line is committed.
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

// Passthrough: the action's own key goes to the pty, encoded - Esc, Ctrl+C, Shift+Tab, Ctrl+arrows reach the app
// even though the editor binds them. Escape-hatch actions return false so the view hands them to the editor.
bool TerminalController::ForwardActionToShell(const EditorAction &kpAction) {
    if (IsTerminalEscapeHatch(kpAction)) {
        return false;
    }
    if ((kpAction.uiAction == kUIAction::kUIActionCommitLine) && doesShellOwnLineEditing) {
        // Bug #17: a dot-command finished in readline is the editor's, not the shell's
        if (CommitShellOwnedDotCommand()) {
            return true;
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
            MarkCommandOutput();
        }
        WriteToPty("\r");
        ExitShellOwned();
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
    if (IsAppActive() || doesShellOwnLineEditing) {
        WriteToPty(gnilk::vt::EncodePaste(bytes, GetModes()));
        return;
    }
    if (text.find_first_of(U"\r\n") == std::u32string::npos) {
        inputLine->Insert(inputCursor.position.x, text);
        inputCursor.position.x += (int)text.length();
        ScrollToBottom();
        return;
    }
    // Several lines: readline takes the line (as on Tab completion), the paste lands in it bracketed - visible
    // and editable, run on Enter
    EnterShellOwned();
    gnilk::vt::Modes bracketed;
    bracketed.isBracketedPaste = true;
    WriteToPty(UnicodeHelper::utf32to8(inputLine->Buffer()) + gnilk::vt::EncodePaste(bytes, bracketed));
    ScrollToBottom();
}

void TerminalController::SetFocus(bool isFocused) {
    auto bytes = gnilk::vt::EncodeFocus(isFocused, GetModes());
    if (!bytes.empty()) {
        WriteToPty(bytes);
    }
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

bool TerminalController::OnAction(const EditorAction &kpAction) {
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

// readline takes the line. Until its echo arrives, inputLine (what was typed) is the line.
void TerminalController::EnterShellOwned() {
    std::lock_guard<std::mutex> guard(screenLock);
    promptAnchor = screen.GetCursorPos();
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
    std::u32string cmdLine(inputLine->Buffer());
    RecordCommand(cmdLine);


    inputLine->Clear();
    inputCursor.position.x = 0;

    // A dot-command runs in the editor - it is no shell command, and leaves the last output alone (`.co` copies
    // the output of the command before it)
    if (PluginExecutor::ParseAndExecuteWithCmdPrefix(cmdLine)) {
        static auto newline = std::u32string(U"\n");
        shell.SendCmd(newline);
    } else {
        if (!cmdLine.empty()) {
            MarkCommandOutput();
        }
        cmdLine += U"\n";
        shell.SendCmd(cmdLine);
    }

    ScrollToBottom();   // §5.3: committing a line snaps the viewport back to the bottom
    Editor::Instance().TriggerUIRedraw();
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
void TerminalController::MarkCommandOutput() {
    std::lock_guard<std::mutex> guard(screenLock);
    if (!useOsc133Boundaries) {
        screen.MarkOutputStart(false);
    }
}

// Console output from the editor (JS Console, plugins) - printed as text on a fresh line. '\n' breaks the line;
// other control characters except TAB show as U+FFFD, so the text can't drive the terminal.
void TerminalController::WriteLine(const std::u32string &str) {
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
    {
        std::lock_guard<std::mutex> guard(screenLock);
        // If mid-line (the shell prompt may have arrived already), start a fresh line first
        if (screen.GetCursorPos().x > 0) {
            screen.Feed("\r\n");
        }
        screen.Feed(UnicodeHelper::utf32to8(text));
        screen.Feed("\r\n");
    }
    Editor::Instance().TriggerUIRedraw();
}

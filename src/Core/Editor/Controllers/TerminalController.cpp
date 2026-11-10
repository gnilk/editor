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

// CommandBlock::Source <-> the persisted int (TerminalBlockSession::source). Explicit (not a raw cast)
// so a future enum reordering can't silently re-map persisted sessions.
static int SourceToInt(TerminalScreen::CommandBlock::Source s) {
    switch (s) {
        case TerminalScreen::CommandBlock::Source::kCommitLine: return 0;
        case TerminalScreen::CommandBlock::Source::kOsc133:     return 1;
        case TerminalScreen::CommandBlock::Source::kHeuristic:  return 2;
    }
    return 2;
}
static TerminalScreen::CommandBlock::Source IntToSource(int v) {
    switch (v) {
        case 0:  return TerminalScreen::CommandBlock::Source::kCommitLine;
        case 1:  return TerminalScreen::CommandBlock::Source::kOsc133;
        default: return TerminalScreen::CommandBlock::Source::kHeuristic;
    }
}

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
    // §4.2 (TS-3c): opt-in OSC 133 shell integration - appends prompt hooks that emit semantic-prompt
    // markers, giving exact command boundaries + exit codes. Off by default (it rewrites PS1).
    if (Config::Instance()["terminal"].GetBool("shell_integration", false)) {
        auto osc133 = BuildShellIntegrationBootstrap(shellBinary);
        shellInitScript.insert(shellInitScript.end(), osc133.begin(), osc133.end());
    }

    // Restore persisted scrollback (terminal-scrollback.md §8.2) BEFORE the shell starts - the pty
    // thread mutates the grid the instant shell.Begin() returns, so the seeded history (and the fresh
    // open loose block covering new output) must already be in place. A no-op when there is no session,
    // no persisted blocks, the feature is off, or the .bin is missing/corrupt.
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
    TerminalScreen::ScrollbackSnapshot snap;
    {
        std::lock_guard<std::mutex> lock(screenLock);
        snap = screen.SnapshotScrollbackTail(cap);
    }
    if (snap.lines.empty()) {
        return false;   // alt-screen, or nothing has scrolled into history yet
    }

    // Serialise line text + ANSI colour spans to the .bin (lossless, versioned - TS-5a). The Line::Refs
    // are shared into a throwaway buffer purely to reach TextBuffer::SaveWithAttributes.
    TextBuffer buffer;
    for (const auto &line : snap.lines) {
        buffer.AddLine(line);
    }
    if (!buffer.SaveWithAttributes(binPath)) {
        if (logger != nullptr) {
            logger->Error("Failed to write terminal scrollback '%s'", binPath.string().c_str());
        }
        return false;
    }

    out.scrollbackFile = binPath.filename().string();
    out.blocks.reserve(snap.blocks.size());
    for (const auto &pb : snap.blocks) {
        TerminalBlockSession tbs;
        tbs.id        = pb.id;
        tbs.command   = UnicodeHelper::utf32to8(pb.command);
        tbs.startLine = pb.startLine;
        tbs.endLine   = pb.endLine;
        tbs.exitCode  = pb.exitCode.has_value() ? pb.exitCode.value()
                                                : TerminalBlockSession::kExitCodeUnknown;
        tbs.cwd       = pb.cwd.string();
        tbs.source    = SourceToInt(pb.source);
        tbs.language  = "";   // Phase 4
        out.blocks.push_back(std::move(tbs));
    }
    return true;
}

void TerminalController::FromSession(const TerminalSession &in) {
    if (in.blocks.empty()) {
        return;   // nothing persisted (fresh project, or the feature was off when last saved)
    }
    if (!Config::Instance()["terminal"].GetBool("persist_scrollback", true)) {
        return;
    }
    auto filename = in.scrollbackFile.empty() ? std::string("terminal_scrollback.bin") : in.scrollbackFile;
    auto binPath  = ResolveScrollbackBinPath(filename);
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

    std::vector<TerminalScreen::PersistedBlock> seedBlocks;
    seedBlocks.reserve(in.blocks.size());
    for (const auto &tbs : in.blocks) {
        TerminalScreen::PersistedBlock pb;
        pb.id        = tbs.id;
        pb.command   = UnicodeHelper::utf8to32(tbs.command);
        pb.startLine = tbs.startLine;
        pb.endLine   = tbs.endLine;
        if (tbs.exitCode != TerminalBlockSession::kExitCodeUnknown) {
            pb.exitCode = tbs.exitCode;
        }
        pb.cwd    = tbs.cwd;
        pb.source = IntToSource(tbs.source);
        seedBlocks.push_back(std::move(pb));
    }

    std::lock_guard<std::mutex> lock(screenLock);
    screen.SeedScrollback(lines, seedBlocks);
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
    std::lock_guard<std::mutex> guard(screenLock);
    screen.Feed(buffer, length);

    // While the shell owns the line (completion in progress), mirror what readline
    // has echoed back into our local inputLine so the model stays truthful.
    if (doesShellOwnLineEditing) {
        SyncInputLineFromGrid();
    }

    Editor::Instance().TriggerUIRedraw();
}

void TerminalController::SyncInputLineFromGrid() {
    // readline owns the line and echoes its edits into the grid. Read the prompt row
    // back from promptAnchor.x (the prompt end) up to the trailing content and mirror
    // it into inputLine. Caret column is the grid cursor relative to the prompt end.
    auto cursorPos = screen.GetCursorPos();
    if (cursorPos.y != promptAnchor.y) {
        // The prompt moved (output scrolled, or a candidate list redrew it lower).
        // Re-anchoring is the ambiguous-completion case — deferred. Leave the mirror
        // as-is for now.
        return;
    }

    const auto &row = screen.GetRow(cursorPos.y);
    int start = std::clamp(promptAnchor.x, 0, (int)row.size());
    int end   = (int)row.size();
    // Trim trailing blanks so we don't carry the rest of the (padded) grid row.
    while (end > start && row[end - 1].ch == U' ') {
        end--;
    }

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
// shell brackets its own commands. A prompt start, B command start, C output start (opens the block, command text
// = the grid between B and here), D;<exit> command end.
void TerminalController::OnSemanticPrompt(char kind, std::string_view params) {
    useOsc133Boundaries = true;
    switch (kind) {
        case 'B':
            osc133CommandStart = screen.GetCursorPos();
            break;
        case 'C':
            screen.BeginCommandBlock(ReadGridText(osc133CommandStart, screen.GetCursorPos()),
                                     TerminalScreen::CommandBlock::Source::kOsc133);
            break;
        case 'D': {
            std::optional<int> exitCode;
            auto code = params.substr(0, params.find(';'));
            if (!code.empty() && (code.find_first_not_of("0123456789") == std::string_view::npos) &&
                (code.size() < 10)) {
                exitCode = std::stoi(std::string(code));
            }
            screen.EndOpenBlock(exitCode);
            break;
        }
        default:
            break;
    }
}

// OSC 7 - "file://host/path": the path is the block's working directory (best effort)
void TerminalController::OnWorkingDirectory(std::string_view uri) {
    std::string_view path = uri;
    static constexpr std::string_view kScheme = "file://";
    if (uri.substr(0, kScheme.size()) == kScheme) {
        auto slash = uri.find('/', kScheme.size());
        path = (slash == std::string_view::npos) ? std::string_view("/") : uri.substr(slash);
    }
    if (!path.empty()) {
        screen.SetOpenBlockCwd(std::filesystem::path(std::string(path)));
    }
}

// Recover text from the grid span [from, to] - the typed command between OSC 133;B and ;C. Joins
// wrapped rows and trims trailing blanks. screenLock is held by the caller (HandleAnsiCmd).
std::u32string TerminalController::ReadGridText(const Point &from, const Point &to) const {
    std::u32string text;
    for (int y = from.y; y <= to.y; y++) {
        if (y < 0 || y >= screen.Rows()) {
            continue;
        }
        const auto &row = screen.GetRow(y);
        int startX = (y == from.y) ? from.x : 0;
        int endX   = (y == to.y)   ? to.x   : (int)row.size();
        startX = std::clamp(startX, 0, (int)row.size());
        endX   = std::clamp(endX,   0, (int)row.size());
        for (int x = startX; x < endX; x++) {
            if (row[x].width != 0) {
                text += row[x].ch;
            }
        }
    }
    while (!text.empty() && text.back() == U' ') {
        text.pop_back();
    }
    return text;
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
        case kAction::kActionCycleActiveEditor:
        case kAction::kActionCycleActiveBufferNext:
        case kAction::kActionCycleActiveBufferPrev:
        case kAction::kActionSwitchToBuffer:
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
    {
        std::lock_guard<std::mutex> guard(screenLock);
        promptAnchor = screen.GetCursorPos();
    }
    doesShellOwnLineEditing = true;
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
        {
            std::lock_guard<std::mutex> guard(screenLock);
            promptAnchor = screen.GetCursorPos();
        }
        doesShellOwnLineEditing = true;
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
        // §5.4: jump to the previous/next command block's start - same "pages the backlog" scope.
        case kUIAction::kUIActionPrevPrompt :
            JumpToPrevPrompt();
            break;
        case kUIAction::kUIActionNextPrompt :
            JumpToNextPrompt();
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

    int64_t next = (int64_t)current + deltaRows;
    next = std::clamp(next, (int64_t)top, (int64_t)bottom);

    anchorAbsRow = (uint64_t)next;
    followBottom = (anchorAbsRow >= bottom);
}

void TerminalController::JumpToPrevPrompt() {
    std::lock_guard<std::mutex> guard(screenLock);
    uint64_t topVisible = CurrentTopVisibleAbsRow();
    const auto &blocks = screen.Blocks();

    uint64_t target = blocks.front().startAbsRow;   // clamp: nothing earlier than the oldest block
    for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
        if (it->startAbsRow < topVisible) {
            target = it->startAbsRow;
            break;
        }
    }
    anchorAbsRow = target;
    followBottom = false;
}

void TerminalController::JumpToNextPrompt() {
    std::lock_guard<std::mutex> guard(screenLock);
    uint64_t topVisible = CurrentTopVisibleAbsRow();
    const auto &blocks = screen.Blocks();

    uint64_t target = blocks.back().startAbsRow;   // clamp: nothing later than the newest (open) block
    for (const auto &block : blocks) {
        if (block.startAbsRow > topVisible) {
            target = block.startAbsRow;
            break;
        }
    }
    anchorAbsRow = target;
    followBottom = false;
}

// §5.5.2: the block "selected" by the viewport - the one containing the top-visible anchor row while
// scrolled. nullopt when following the bottom (at the live prompt, nothing selected) or when the anchor
// falls in no block. Assumes screenLock is held (reads Blocks()/AbsRowCount()); the view holds it,
// single-threaded tests need none - same contract as CurrentTopVisibleAbsRow.
std::optional<size_t> TerminalController::SelectedBlockIndex() const {
    if (followBottom) {
        return std::nullopt;
    }
    const auto &blocks = screen.Blocks();
    for (size_t i = 0; i < blocks.size(); i++) {
        const auto &block = blocks[i];
        uint64_t end = block.endAbsRow.value_or(screen.AbsRowCount());   // open block runs to the live row
        if (anchorAbsRow >= block.startAbsRow && anchorAbsRow < end) {
            return i;
        }
    }
    return std::nullopt;
}

// Join a block's per-row output (GetBlockOutputText, TS-2a) into one '\n'-separated string, or nullopt
// when the id matches no block. Assumes screenLock is held by the caller.
static std::optional<std::u32string> JoinBlockOutput(const TerminalScreen &screen, uint64_t blockId) {
    bool found = false;
    for (const auto &block : screen.Blocks()) {
        if (block.id == blockId) {
            found = true;
            break;
        }
    }
    if (!found) {
        return std::nullopt;
    }
    auto lines = screen.GetBlockOutputText(blockId);
    std::u32string text;
    for (size_t i = 0; i < lines.size(); i++) {
        if (i > 0) {
            text += U"\n";
        }
        text += lines[i];
    }
    return text;
}

// Project a model CommandBlock onto the engine-agnostic OutputBlockInfo (TS-2c). Assumes screenLock
// is held (reads AbsRowCount for the open block's end).
static IOutputConsole::OutputBlockInfo ToBlockInfo(const TerminalScreen &screen,
                                                   const TerminalScreen::CommandBlock &block) {
    IOutputConsole::OutputBlockInfo info;
    info.id = block.id;
    info.command = block.command;
    info.exitCode = block.exitCode;
    uint64_t end = block.endAbsRow.value_or(screen.AbsRowCount());   // open block runs to the live row
    info.lineCount = (end > block.startAbsRow) ? (end - block.startAbsRow) : 0;
    return info;
}

std::optional<std::u32string> TerminalController::GetSelectedBlockText() {
    std::lock_guard<std::mutex> guard(screenLock);
    auto sel = SelectedBlockIndex();   // safe: we hold screenLock (its documented precondition)
    if (!sel.has_value()) {
        return std::nullopt;
    }
    return JoinBlockOutput(screen, screen.Blocks()[*sel].id);
}

std::vector<IOutputConsole::OutputBlockInfo> TerminalController::GetBlocks() {
    std::lock_guard<std::mutex> guard(screenLock);
    std::vector<OutputBlockInfo> out;
    const auto &blocks = screen.Blocks();
    out.reserve(blocks.size());
    for (const auto &block : blocks) {
        out.push_back(ToBlockInfo(screen, block));
    }
    return out;
}

std::optional<std::u32string> TerminalController::GetBlockOutputText(uint64_t blockId) {
    std::lock_guard<std::mutex> guard(screenLock);
    return JoinBlockOutput(screen, blockId);
}

std::optional<IOutputConsole::OutputBlockInfo> TerminalController::GetLastBlock() {
    std::lock_guard<std::mutex> guard(screenLock);
    const auto &blocks = screen.Blocks();
    if (blocks.empty()) {   // never happens (the ctor opens the loose block), but stay total
        return std::nullopt;
    }
    return ToBlockInfo(screen, blocks.back());
}

void TerminalController::ExitShellOwned() {
    // Return to local line editing. The committed/aborted line lives in the shell's
    // output now; our local buffer starts fresh at the next prompt.
    doesShellOwnLineEditing = false;
    inputLine->Clear();
    inputCursor.position.x = 0;
}

int TerminalController::GetCursorXPos() {
    return screen.GetCursorPos().x + inputCursor.position.x;
}

void TerminalController::CommitLine() {
    std::u32string cmdLine(inputLine->Buffer());
    RecordCommand(cmdLine);


    inputLine->Clear();
    inputCursor.position.x = 0;

    if (PluginExecutor::ParseAndExecuteWithCmdPrefix(cmdLine)) {
        static auto newline = std::u32string(U"\n");
        shell.SendCmd(newline);
    } else {
        cmdLine += U"\n";
        shell.SendCmd(cmdLine);
    }

    ScrollToBottom();   // §5.3: committing a line snaps the viewport back to the bottom
    Editor::Instance().TriggerUIRedraw();
}

// A committed command line - typed locally (CommitLine) or finished in readline after Tab (ForwardActionToShell):
// into the history, saved, and (without OSC 133) opens its command block. Call without screenLock held.
void TerminalController::RecordCommand(const std::u32string &cmdLine) {
    if (!cmdLine.empty()) {
        cmdHistory.Push(cmdLine);
        if (!cmdHistoryPath.empty()) {
            cmdHistory.Save(cmdHistoryPath);
        }
        // §4.1: every non-empty commit opens a block - plugin/built-in commands included (their
        // output still lands in scrollback via WriteLine, §4.3; v1 leaves them coarser, closed by
        // whatever commits next rather than a dedicated end event - producer-side bracketing is
        // deferred, §4.3). BeginCommandBlock closes whatever block was previously open and no-ops in
        // alt-screen (§10). Only this part runs under screenLock (shell.SendCmd must not).
        std::lock_guard<std::mutex> guard(screenLock);
        // §4.2: once OSC 133 shell integration is active the shell opens the block itself at ;C, so the
        // heuristic stands down to avoid a double-open (flag read under screenLock, set on the pty thread).
        if (!useOsc133Boundaries) {
            screen.BeginCommandBlock(cmdLine, TerminalScreen::CommandBlock::Source::kCommitLine);
        }
    }
    cmdHistory.ResetNavigation();
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

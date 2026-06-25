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

// 256-colour palette — first 16 entries from kitty, rest initialised in InitializeColorTable()
static ColorRGBA FG_BG_256[256] = {
    ColorRGBA::FromRGB(0x00, 0x00, 0x00),   // 0
    ColorRGBA::FromRGB(0xcd, 0x00, 0x00),   // 1
    ColorRGBA::FromRGB(0x00, 0xcd, 0x00),   // 2
    ColorRGBA::FromRGB(0xcd, 0xcd, 0x00),   // 3
    ColorRGBA::FromRGB(0x00, 0x00, 0xee),   // 4
    ColorRGBA::FromRGB(0xcd, 0x00, 0xcd),   // 5
    ColorRGBA::FromRGB(0x00, 0xcd, 0xcd),   // 6
    ColorRGBA::FromRGB(0xe5, 0xe5, 0xe5),   // 7
    ColorRGBA::FromRGB(0x7f, 0x7f, 0x7f),   // 8
    ColorRGBA::FromRGB(0xff, 0x00, 0x00),   // 9
    ColorRGBA::FromRGB(0x00, 0xff, 0x00),   // 10
    ColorRGBA::FromRGB(0xff, 0xff, 0x00),   // 11
    ColorRGBA::FromRGB(0x5c, 0x5c, 0xff),   // 12
    ColorRGBA::FromRGB(0xff, 0x00, 0xff),   // 13
    ColorRGBA::FromRGB(0x00, 0xff, 0xff),   // 14
    ColorRGBA::FromRGB(0xff, 0xff, 0xff),   // 15
};

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

void TerminalController::InitializeColorTable() {
    const uint8_t valuerange[6] = {0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff};
    uint8_t j = 16;
    for (uint8_t i = 0; i < 216; i++, j++) {
        auto r = valuerange[(i / 36) % 6];
        auto g = valuerange[(i / 6)  % 6];
        auto b = valuerange[i % 6];
        FG_BG_256[j] = ColorRGBA::FromRGB(r, g, b);
    }
    for (uint8_t i = 0; i < 24; i++, j++) {
        uint8_t v = 8 + i * 10;
        FG_BG_256[j] = ColorRGBA::FromRGB(v, v, v);
    }
}

void TerminalController::Begin() {
    logger = gnilk::Logger::GetLogger("TerminalController");
    logger->Debug("Begin");

    RegisterAsOutputConsole();

    inputLine = std::make_shared<Line>();
    inputCursor.position.x = 0;

    InitializeColorTable();

    shell.SetOutputDelegate([this](const uint8_t *buffer, size_t length) {
        HandleTerminalData(buffer, length);
    });

    auto shellBinary    = Config::Instance()["terminal"].GetStr("shell", "/bin/bash");
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

    shell.Begin(shellBinary, shellInitStr, shellInitScript);

    while (shell.GetState() != Shell::State::kRunning) {
        if (shell.GetState() == Shell::State::kTerminated) {
            logger->Error("Shell got terminated while starting!");
            return;
        }
        std::this_thread::yield();
    }

    // A '.goatedit' marker directory in the project registers a kProject search path; its
    // presence opts the project into project-local history. Otherwise history is per-user.
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto projectPath = assetLoader.ResolveWritePath("terminal_history", AssetLoaderBase::kLocationType::kProject);
    auto histLocation = projectPath.empty() ? AssetLoaderBase::kLocationType::kUser
                                            : AssetLoaderBase::kLocationType::kProject;

    auto histAsset = assetLoader.LoadTextAsset("terminal_history", histLocation);
    cmdHistory.Load(histAsset);

    // Save back to wherever it loaded from; on first run (no asset) resolve a write path.
    cmdHistoryPath = (histAsset != nullptr)
                    ? histAsset->GetOriginPath()
                    : assetLoader.ResolveWritePath("terminal_history", histLocation);
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
    auto termColors = Editor::Instance().GetTheme()->GetTerminalColor();
    std::lock_guard<std::mutex> guard(screenLock);
    // SetDefaultColors first so blank cells created by Resize get the right colors
    screen.SetDefaultColors(termColors.GetColor("foreground"), termColors.GetColor("background"));
    screen.Resize(cols, rows);
    shell.SetWindowSize(cols, rows);
}

void TerminalController::HandleTerminalData(const uint8_t *buffer, size_t length) {
    VTermParser vtParser;
    auto stripped = vtParser.Parse(buffer, length);
    auto &cmdBuffer = vtParser.LastCmdBuffer();

    std::lock_guard<std::mutex> guard(screenLock);

    size_t idxCmd = 0;
    for (size_t i = 0; i < stripped.size(); i++) {
        // Apply all commands whose position falls at this character
        while (idxCmd < cmdBuffer.size() && cmdBuffer[idxCmd].idxString == i) {
            HandleAnsiCmd(cmdBuffer[idxCmd]);
            idxCmd++;
        }

        auto ch = static_cast<uint8_t>(stripped[i]);
        if (ch == 0x08) {
            // Backspace is non-destructive: it only moves the cursor left. readline
            // erases by sending "\b \b" (left, space-over, left), so the actual blanking
            // is done by the space; we just must not drop the \b or the cursor desyncs.
            screen.MoveCursor(-1, 0);
        } else if (ch == 0x0a) {
            screen.NewLine();
        } else if (ch == 0x0d) {
            screen.CarriageReturn();
        } else if (ch >= 0x20 && ch < 0x7f) {
            screen.PutChar(static_cast<char32_t>(ch));
        } else if (ch >= 0x80) {
            std::u32string u32;
            int n = UnicodeHelper::ConvertUTF8ToUTF32Char(
                u32, reinterpret_cast<const uint8_t *>(&stripped[i]), stripped.size() - i);
            if (n > 0) {
                for (auto c : u32) {
                    screen.PutChar(c);
                }
                i += (n - 1);
            } else {
                screen.PutChar(U'.');
            }
        }
    }
    // Drain any trailing commands
    while (idxCmd < cmdBuffer.size()) {
        HandleAnsiCmd(cmdBuffer[idxCmd++]);
    }

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
        lineText += row[x].ch;
    }

    inputLine->Clear();
    inputLine->Append(lineText);
    inputCursor.position.x = std::clamp(cursorPos.x - promptAnchor.x, 0, (int)inputLine->Length());
}

void TerminalController::HandleAnsiCmd(const VTermParser::CMD &cmd) {
    auto termColors = Editor::Instance().GetTheme()->GetTerminalColor();
    auto P = [&](int i, int def = 1) -> int {
        return (i < (int)cmd.param.size()) ? cmd.param[i] : def;
    };

    switch (cmd.cmd) {
        // --- SGR ---
        case VTermParser::kAnsiCmd::kSGRReset :
            screen.ResetAttributes();
            break;
        case VTermParser::kAnsiCmd::kFontBold :
            screen.SetAttributes(TerminalScreen::kAttrBold);
            break;
        case VTermParser::kAnsiCmd::kFontItalic :
            screen.SetAttributes(TerminalScreen::kAttrItalic);
            break;
        case VTermParser::kAnsiCmd::kFontUnderline :
            screen.SetAttributes(TerminalScreen::kAttrUnderline);
            break;
        case VTermParser::kAnsiCmd::kInvertColors :
            screen.SetAttributes(TerminalScreen::kAttrInvert);
            break;
        case VTermParser::kAnsiCmd::kFontNormal :
            screen.SetAttributes(0);
            break;
        case VTermParser::kAnsiCmd::kSetForegroundColor :
            if (!cmd.param.empty()) {
                auto idx = (cmd.param[0] & 7) + 8;
                screen.SetForeground(termColors.GetColor(std::to_string(idx)));
            }
            break;
        case VTermParser::kAnsiCmd::kSetBackgroundColor :
            if (!cmd.param.empty()) {
                auto idx = cmd.param[0] & 7;
                screen.SetBackground(termColors.GetColor(std::to_string(idx)));
            }
            break;
        case VTermParser::kAnsiCmd::kSetForeground256 :
            if (!cmd.param.empty()) {
                int idx = std::clamp(cmd.param[0], 0, 255);
                screen.SetForeground(FG_BG_256[idx]);
            }
            break;
        case VTermParser::kAnsiCmd::kSetBackground256 :
            if (!cmd.param.empty()) {
                int idx = std::clamp(cmd.param[0], 0, 255);
                screen.SetBackground(FG_BG_256[idx]);
            }
            break;
        case VTermParser::kAnsiCmd::kSetDefaultForegroundColor :
            screen.SetForeground(termColors.GetColor("foreground"));
            break;
        case VTermParser::kAnsiCmd::kSetDefaultBackgroundColor :
            screen.SetBackground(termColors.GetColor("background"));
            break;

        // --- Cursor movement ---
        case VTermParser::kAnsiCmd::kCursorUp :
            screen.MoveCursor(0, -P(0));
            break;
        case VTermParser::kAnsiCmd::kCursorDown :
            screen.MoveCursor(0, P(0));
            break;
        case VTermParser::kAnsiCmd::kCursorForward :
            screen.MoveCursor(P(0), 0);
            break;
        case VTermParser::kAnsiCmd::kCursorBack :
            screen.MoveCursor(-P(0), 0);
            break;
        case VTermParser::kAnsiCmd::kCursorPos :
            // ANSI is 1-indexed; convert to 0-indexed
            screen.SetCursorPos(P(1, 1) - 1, P(0, 1) - 1);
            break;

        // --- Erase ---
        case VTermParser::kAnsiCmd::kEraseInLine :
            screen.EraseInLine(P(0, 0));
            break;
        case VTermParser::kAnsiCmd::kEraseInDisplay :
            screen.EraseInDisplay(P(0, 0));
            break;

        // --- Cursor save/restore ---
        case VTermParser::kAnsiCmd::kSaveCursor :
            screen.SaveCursorPos();
            break;
        case VTermParser::kAnsiCmd::kRestoreCursor :
            screen.RestoreCursorPos();
            break;

        // --- Scroll region (1-indexed in the sequence, 0-indexed in TerminalScreen) ---
        case VTermParser::kAnsiCmd::kSetScrollRegion :
            screen.SetScrollRegion(P(0, 1) - 1, P(1, screen.Rows()) - 1);
            break;

        // --- Alternate screen ---
        case VTermParser::kAnsiCmd::kEnterAltScreen :
            screen.SaveScreen();
            break;
        case VTermParser::kAnsiCmd::kLeaveAltScreen :
            screen.RestoreScreen();
            break;

        // --- Terminal responses (write back to the pty) ---
        case VTermParser::kAnsiCmd::kDeviceStatusReport : {
            auto pos = screen.GetCursorPos();
            char resp[32];
            snprintf(resp, sizeof(resp), "\x1b[%d;%dR", pos.y + 1, pos.x + 1);
            shell.WriteBytes(resp);
            break;
        }
        case VTermParser::kAnsiCmd::kPrimaryDA :
            shell.WriteBytes("\x1b[?1;2c");
            break;

        // --- Full-screen app line/char operations ---
        case VTermParser::kAnsiCmd::kReverseIndex :
            screen.ReverseIndex();
            break;
        case VTermParser::kAnsiCmd::kInsertLine :
            screen.InsertLine(P(0, 1));
            break;
        case VTermParser::kAnsiCmd::kDeleteLine :
            screen.DeleteLine(P(0, 1));
            break;
        case VTermParser::kAnsiCmd::kInsertChar :
            screen.InsertChar(P(0, 1));
            break;
        case VTermParser::kAnsiCmd::kDeleteChar :
            screen.DeleteChar(P(0, 1));
            break;

        // --- Cursor key mode ---
        case VTermParser::kAnsiCmd::kCursorKeyModeApp :
            cursorKeyAppMode = true;
            break;
        case VTermParser::kAnsiCmd::kCursorKeyModeNormal :
            cursorKeyAppMode = false;
            break;

        // --- Cursor visibility (not yet tracked — silently consumed) ---
        case VTermParser::kAnsiCmd::kCursorShow :
        case VTermParser::kAnsiCmd::kCursorHide :
            break;

        // --- OSC 133 shell integration (§4.2) ---
        // Seeing ANY marker means the shell brackets its own commands, so the CommitLine heuristic
        // stands down (useOsc133Boundaries) and OSC 133 drives the block boundaries from here.
        case VTermParser::kAnsiCmd::kPromptStart :
            useOsc133Boundaries = true;
            break;
        case VTermParser::kAnsiCmd::kCommandStart :
            // End of prompt: the typed command starts at the cursor; record it for ;C to read back.
            useOsc133Boundaries = true;
            osc133CommandStart = screen.GetCursorPos();
            break;
        case VTermParser::kAnsiCmd::kOutputStart :
            // Output start: open the block, command text = the grid span [;B .. here].
            useOsc133Boundaries = true;
            screen.BeginCommandBlock(ReadGridText(osc133CommandStart, screen.GetCursorPos()),
                                     TerminalScreen::CommandBlock::Source::kOsc133);
            break;
        case VTermParser::kAnsiCmd::kCommandEnd : {
            // Command finished: close the open block with the reported exit code (-1 == unknown).
            useOsc133Boundaries = true;
            std::optional<int> exitCode;
            if (!cmd.param.empty() && cmd.param[0] >= 0) {
                exitCode = cmd.param[0];
            }
            screen.EndOpenBlock(exitCode);
            break;
        }
        case VTermParser::kAnsiCmd::kSetCwd :
            // OSC 7: best-effort cwd for the open block.
            if (!cmd.strParam.empty()) {
                screen.SetOpenBlockCwd(std::filesystem::path(cmd.strParam));
            }
            break;
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
            text += row[x].ch;
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

    // (1) Full-screen app owns the grid (vi/less requested the alt-screen via ANSI). This wins
    //     over everything else - while a full-screen app is up, every key is its input.
    if (screen.IsAltScreen()) {
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

bool TerminalController::ForwardKeyPressToShell(const KeyPress &keyPress) {
    if (keyPress.IsHumanReadable()) {
        uint8_t ch = (uint8_t)keyPress.key;
        if (keyPress.IsCtrlPressed()) {
            if (keyPress.key >= 'a' && keyPress.key <= 'z') {
                ch = (uint8_t)(keyPress.key - 'a' + 1);
            } else if (keyPress.key >= 'A' && keyPress.key <= 'Z') {
                ch = (uint8_t)(keyPress.key - 'A' + 1);
            } else if (keyPress.key == '[') {
                ch = 0x1b;  // Ctrl+[ == ESC
            }
        }
        shell.Write(ch);
        return true;
    }
    if (!keyPress.isSpecialKey) {
        return false;
    }
    const char *seq = nullptr;
    switch (keyPress.specialKey) {
        case Keyboard::kKeyCode_Return:    shell.Write(0x0d); return true;
        case Keyboard::kKeyCode_Escape:    shell.Write(0x1b); return true;
        case Keyboard::kKeyCode_Backspace: shell.Write(0x7f); return true;
        case Keyboard::kKeyCode_Tab:       shell.Write(0x09); return true;
        case Keyboard::kKeyCode_UpArrow:    seq = cursorKeyAppMode ? "\x1bOA" : "\x1b[A"; break;
        case Keyboard::kKeyCode_DownArrow:  seq = cursorKeyAppMode ? "\x1bOB" : "\x1b[B"; break;
        case Keyboard::kKeyCode_RightArrow: seq = cursorKeyAppMode ? "\x1bOC" : "\x1b[C"; break;
        case Keyboard::kKeyCode_LeftArrow:  seq = cursorKeyAppMode ? "\x1bOD" : "\x1b[D"; break;
        case Keyboard::kKeyCode_Home:       seq = "\x1b[H";   break;
        case Keyboard::kKeyCode_End:        seq = "\x1b[F";   break;
        case Keyboard::kKeyCode_PageUp:     seq = "\x1b[5~";  break;
        case Keyboard::kKeyCode_PageDown:   seq = "\x1b[6~";  break;
        case Keyboard::kKeyCode_Insert:     seq = "\x1b[2~";  break;
        case Keyboard::kKeyCode_DeleteForward: seq = "\x1b[3~"; break;
        case Keyboard::kKeyCode_F1:  seq = "\x1bOP";    break;
        case Keyboard::kKeyCode_F2:  seq = "\x1bOQ";    break;
        case Keyboard::kKeyCode_F3:  seq = "\x1bOR";    break;
        case Keyboard::kKeyCode_F4:  seq = "\x1bOS";    break;
        case Keyboard::kKeyCode_F5:  seq = "\x1b[15~";  break;
        case Keyboard::kKeyCode_F6:  seq = "\x1b[17~";  break;
        case Keyboard::kKeyCode_F7:  seq = "\x1b[18~";  break;
        case Keyboard::kKeyCode_F8:  seq = "\x1b[19~";  break;
        case Keyboard::kKeyCode_F9:  seq = "\x1b[20~";  break;
        case Keyboard::kKeyCode_F10: seq = "\x1b[21~";  break;
        case Keyboard::kKeyCode_F11: seq = "\x1b[23~";  break;
        case Keyboard::kKeyCode_F12: seq = "\x1b[24~";  break;
        default: return false;
    }
    if (seq != nullptr) {
        shell.WriteBytes(seq);
    }
    return true;
}

bool TerminalController::ForwardActionToShell(const EditorAction &kpAction) {
    const char *seq = nullptr;
    if (kpAction.action == kAction::kActionShellCompletion) {
        shell.Write(0x09);
        return true;
    }
    switch (kpAction.uiAction) {
        case kUIAction::kUIActionCommitLine:
            shell.Write(0x0d);
            // When the shell owns line editing the line lives in readline; committing hands
            // control back and the shell will print its output + a fresh prompt.
            if (doesShellOwnLineEditing) {
                // §4.1 tab-completion edge case: this commit never goes through CommitLine(), so the
                // block boundary has to open here instead. SyncInputLineFromGrid has been mirroring
                // readline's edits into inputLine, so it holds the best-effort command text - read it
                // before ExitShellOwned() clears it.
                std::u32string cmdLine(inputLine->Buffer());
                if (!cmdLine.empty()) {
                    std::lock_guard<std::mutex> guard(screenLock);
                    if (!useOsc133Boundaries) {   // §4.2: OSC 133;C opens the block when integration is active
                        screen.BeginCommandBlock(cmdLine, TerminalScreen::CommandBlock::Source::kCommitLine);
                    }
                }
                ExitShellOwned();
            }
            return true;
        case kUIAction::kUIActionLineLeft:    seq = cursorKeyAppMode ? "\x1bOD" : "\x1b[D"; break;
        case kUIAction::kUIActionLineRight:   seq = cursorKeyAppMode ? "\x1bOC" : "\x1b[C"; break;
        case kUIAction::kUIActionLineUp:      seq = cursorKeyAppMode ? "\x1bOA" : "\x1b[A"; break;
        case kUIAction::kUIActionLineDown:    seq = cursorKeyAppMode ? "\x1bOB" : "\x1b[B"; break;
        case kUIAction::kUIActionLineHome:    seq = "\x1b[H";  break;
        case kUIAction::kUIActionLineEnd:     seq = "\x1b[F";  break;
        case kUIAction::kUIActionPageUp:      seq = "\x1b[5~"; break;
        case kUIAction::kUIActionPageDown:    seq = "\x1b[6~"; break;
        default: return true;  // swallow — don't let editor-level actions fire
    }
    if (seq != nullptr) {
        shell.WriteBytes(seq);
    }
    return true;
}

bool TerminalController::OnAction(const EditorAction &kpAction) {
    if (kpAction.action == kAction::kActionShellCompletion) {
        // First Tab in local-edit mode: hand the line over to readline so it can
        // complete. The real grid cursor has stayed parked at the prompt end the
        // whole time we edited locally (local edits never touch the grid), so
        // capture it now as the prompt anchor for reading the line back. From here
        // readline owns the line until the command is committed or aborted.
        promptAnchor = screen.GetCursorPos();
        doesShellOwnLineEditing = true;
        auto cmdLine = std::u32string(inputLine->Buffer());
        if (!cmdLine.empty()) {
            shell.SendCmd(cmdLine);
        }
        shell.Write(0x09);
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

    if (!cmdLine.empty()) {
        cmdHistory.Push(cmdLine);
        if (!cmdHistoryPath.empty()) {
            cmdHistory.Save(cmdHistoryPath);
        }
        // §4.1: every non-empty commit opens a block - plugin/built-in commands included (their
        // output still lands in scrollback via WriteLine, §4.3; v1 leaves them coarser, closed by
        // whatever commits next rather than a dedicated end event - producer-side bracketing is
        // deferred, §4.3). BeginCommandBlock closes whatever block was previously open and no-ops in
        // alt-screen (§10); mirrors WriteLine's short locked section rather than locking the whole
        // function (shell.SendCmd below must not run under screenLock).
        std::lock_guard<std::mutex> guard(screenLock);
        // §4.2: once OSC 133 shell integration is active the shell opens the block itself at ;C, so the
        // heuristic stands down to avoid a double-open (flag read under screenLock, set on the pty thread).
        if (!useOsc133Boundaries) {
            screen.BeginCommandBlock(cmdLine, TerminalScreen::CommandBlock::Source::kCommitLine);
        }
    }
    cmdHistory.ResetNavigation();

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

void TerminalController::WriteLine(const std::u32string &str) {
    std::lock_guard<std::mutex> guard(screenLock);
    // If mid-line (shell prompt may have already arrived), move to a fresh line first.
    if (screen.GetCursorPos().x > 0) {
        screen.CarriageReturn();
        screen.NewLine();
    }
    for (auto ch : str) {
        screen.PutChar(ch);
    }
    screen.CarriageReturn();
    screen.NewLine();
    Editor::Instance().TriggerUIRedraw();
}

//
// Created by gnilk on 22.02.2024.
//

#include "TerminalController.h"
#include "Core/Editor.h"
#include "Core/HexDump.h"
#include "Core/Plugins/PluginExecutor.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

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

    RuntimeConfig::Instance().SetOutputConsole(this);

    inputLine = std::make_shared<Line>();
    inputCursor.position.x = 0;

    InitializeColorTable();

    shell.SetOutputDelegate([this](const uint8_t *buffer, size_t length) {
        HandleTerminalData(buffer, length);
    });

    auto shellBinary    = Config::Instance()["terminal"].GetStr("shell", "/bin/bash");
    auto shellInitStr   = Config::Instance()["terminal"].GetStr("init", "-ils");
    auto shellInitScript = Config::Instance()["terminal"].GetSequenceOfStr("bootstrap");
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
    history.Load(histAsset);

    // Save back to wherever it loaded from; on first run (no asset) resolve a write path.
    historyPath = (histAsset != nullptr)
                    ? histAsset->GetOriginPath()
                    : assetLoader.ResolveWritePath("terminal_history", histLocation);
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
    }
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

bool TerminalController::ForwardActionToShell(const KeyPressAction &kpAction) {
    const char *seq = nullptr;
    switch (kpAction.action) {
        case kAction::kActionCommitLine:
            shell.Write(0x0d);
            // When the shell owns line editing the line lives in readline; committing hands
            // control back and the shell will print its output + a fresh prompt.
            if (doesShellOwnLineEditing) {
                ExitShellOwned();
            }
            return true;
        case kAction::kActionShellCompletion: shell.Write(0x09); return true;
        case kAction::kActionLineLeft:    seq = cursorKeyAppMode ? "\x1bOD" : "\x1b[D"; break;
        case kAction::kActionLineRight:   seq = cursorKeyAppMode ? "\x1bOC" : "\x1b[C"; break;
        case kAction::kActionLineUp:      seq = cursorKeyAppMode ? "\x1bOA" : "\x1b[A"; break;
        case kAction::kActionLineDown:    seq = cursorKeyAppMode ? "\x1bOB" : "\x1b[B"; break;
        case kAction::kActionLineHome:    seq = "\x1b[H";  break;
        case kAction::kActionLineEnd:     seq = "\x1b[F";  break;
        case kAction::kActionPageUp:      seq = "\x1b[5~"; break;
        case kAction::kActionPageDown:    seq = "\x1b[6~"; break;
        default: return true;  // swallow — don't let editor-level actions fire
    }
    if (seq != nullptr) {
        shell.WriteBytes(seq);
    }
    return true;
}

bool TerminalController::OnAction(const KeyPressAction &kpAction) {
    switch (kpAction.action) {
        case kAction::kActionLineHome :
            inputCursor.position.x = 0;
            break;
        case kAction::kActionLineEnd :
            inputCursor.position.x = inputLine->Length();
            break;
        case kAction::kActionLineLeft :
            inputCursor.position.x = std::max(0, inputCursor.position.x - 1);
            break;
        case kAction::kActionLineRight :
            inputCursor.position.x = std::min((int)inputLine->Length(), inputCursor.position.x + 1);
            break;
        case kAction::kActionLineUp : {
            auto entry = history.NavigateUp();
            if (entry.has_value()) {
                inputLine->Clear();
                inputLine->Append(entry.value());
                inputCursor.position.x = (int)inputLine->Length();
            }
            break;
        }
        case kAction::kActionLineDown : {
            auto entry = history.NavigateDown();
            inputLine->Clear();
            if (entry.has_value()) {
                inputLine->Append(entry.value());
            }
            inputCursor.position.x = (int)inputLine->Length();
            break;
        }
        case kAction::kActionShellCompletion : {
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
        default:
            return false;
    }
    Editor::Instance().TriggerUIRedraw();
    return true;
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
        history.Push(cmdLine);
        if (!historyPath.empty()) {
            history.Save(historyPath);
        }
    }
    history.ResetNavigation();

    inputLine->Clear();
    inputCursor.position.x = 0;

    if (PluginExecutor::ParseAndExecuteWithCmdPrefix(cmdLine)) {
        static auto newline = std::u32string(U"\n");
        shell.SendCmd(newline);
    } else {
        cmdLine += U"\n";
        shell.SendCmd(cmdLine);
    }

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

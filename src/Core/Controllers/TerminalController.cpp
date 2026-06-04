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

    auto shellStdHandler = [this](Shell::Stream stream, const uint8_t *buffer, size_t length) {
        HandleTerminalData(buffer, length);
    };
    shell.SetStdoutDelegate(shellStdHandler);
    shell.SetStderrDelegate(shellStdHandler);

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
}

void TerminalController::Resize(int cols, int rows) {
    auto termColors = Editor::Instance().GetTheme()->GetTerminalColor();
    std::lock_guard<std::mutex> guard(screenLock);
    // SetDefaultColors first so blank cells created by Resize get the right colors
    screen.SetDefaultColors(termColors.GetColor("foreground"), termColors.GetColor("background"));
    screen.Resize(cols, rows);
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
            ApplyCommand(cmdBuffer[idxCmd]);
            idxCmd++;
        }

        auto ch = static_cast<uint8_t>(stripped[i]);
        if (ch == 0x0a) {
            // Shell output comes through regular pipes (not the pty slave), so the
            // pty's ONLCR translation never runs. Simulate it here: \n implies \r\n.
            // \r\n sequences are harmless — the redundant CR is a no-op.
            screen.CarriageReturn();
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
        ApplyCommand(cmdBuffer[idxCmd++]);
    }

    Editor::Instance().TriggerUIRedraw();
}

void TerminalController::ApplyCommand(const VTermParser::CMD &cmd) {
    auto termColors = Editor::Instance().GetTheme()->GetTerminalColor();
    switch (cmd.cmd) {
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
        case VTermParser::kAnsiCmd::kSetDefaultForegroundColor :
            screen.SetForeground(termColors.GetColor("foreground"));
            break;
        case VTermParser::kAnsiCmd::kSetDefaultBackgroundColor :
            screen.SetBackground(termColors.GetColor("background"));
            break;
    }
}

bool TerminalController::HandleKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress) {
    if (DefaultEditLine(inputCursor, inputLine, keyPress)) {
        cursor.position.x = GetCursorXPos();
        return true;
    }
    return false;
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
        default:
            return false;
    }
    Editor::Instance().TriggerUIRedraw();
    return true;
}

int TerminalController::GetCursorXPos() {
    return screen.GetCursorPos().x + inputCursor.position.x;
}

void TerminalController::CommitLine() {
    std::u32string cmdLine(inputLine->Buffer());
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
    for (auto ch : str) {
        screen.PutChar(ch);
    }
    screen.CarriageReturn();
    screen.NewLine();
    Editor::Instance().TriggerUIRedraw();
}

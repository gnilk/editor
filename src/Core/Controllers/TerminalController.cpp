//
// Created by gnilk on 22.02.2024.
//

#include "TerminalController.h"
#include "Core/Editor.h"
#include "Core/HexDump.h"
#include "Core/Plugins/PluginExecutor.h"
#include "Core/VTermParser.h"

using namespace gedit;

void TerminalController::Begin() {
    logger = gnilk::Logger::GetLogger("TerminalController");
    logger->Debug("Begin");

    RuntimeConfig::Instance().SetOutputConsole(this);


    inputLine = std::make_shared<Line>();
    inputCursor.position.x = 0;

    auto shellStdHandler= [this](Shell::Stream stream, const uint8_t *buffer, size_t length) {
        HandleTerminalData(buffer, length);
    };
    // Create the first line, we need one to consume data..
    NewLine();

    InitializeColorTable();

    shell.SetStdoutDelegate(shellStdHandler);
    shell.SetStderrDelegate(shellStdHandler);
    auto shellBinary = Config::Instance()["terminal"].GetStr("shell","/bin/bash");
    auto shellInitStr = Config::Instance()["terminal"].GetStr("init", "-ils");
    auto shellInitScript = Config::Instance()["terminal"].GetSequenceOfStr("bootstrap");
    shell.Begin(shellBinary, shellInitStr, shellInitScript);
    //shell.Begin("/bin/bash", "-ils", {});
    while(shell.GetState() != Shell::State::kRunning) {
        if (shell.GetState() == Shell::State::kTerminated) {
            logger->Error("Shell got terminated while starting!");
            return;
        }
        std::this_thread::yield();
    }
}

// From kitty
static ColorRGBA FG_BG_256[256]={
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
/*

static uint32_t FG_BG_256[256] = {
    0x000000,  // 0
    0xcd0000,  // 1
    0x00cd00,  // 2
    0xcdcd00,  // 3
    0x0000ee,  // 4
    0xcd00cd,  // 5
    0x00cdcd,  // 6
    0xe5e5e5,  // 7
    0x7f7f7f,  // 8
    0xff0000,  // 9
    0x00ff00,  // 10
    0xffff00,  // 11
    0x5c5cff,  // 12
    0xff00ff,  // 13
    0x00ffff,  // 14
    0xffffff,  // 15
};

static void
init_FG_BG_table(void) {
    if (UNLIKELY(FG_BG_256[255] == 0)) {
        // colors 16..232: the 6x6x6 color cube
        const uint8_t valuerange[6] = {0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff};
        uint8_t i, j=16;
        for(i = 0; i < 216; i++, j++) {
            uint8_t r = valuerange[(i / 36) % 6], g = valuerange[(i / 6) % 6], b = valuerange[i % 6];
            FG_BG_256[j] = (r << 16) | (g << 8) | b;
        }
        // colors 232..255: grayscale
        for(i = 0; i < 24; i++, j++) {
            uint8_t v = 8 + i * 10;
            FG_BG_256[j] = (v << 16) | (v << 8) | v;
        }
    }
}

 */

void TerminalController::InitializeColorTable() {

    // This is from kitty...
    const uint8_t valuerange[6] = {0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff};
    uint8_t i, j=16;
    for(i = 0; i < 216; i++, j++) {
        auto r = valuerange[(i / 36) % 6];
        auto g = valuerange[(i / 6) % 6];
        auto b = valuerange[i % 6];
        FG_BG_256[j] = ColorRGBA::FromRGB(r,g,b);
    }
    // colors 232..255: grayscale
    for(i = 0; i < 24; i++, j++) {
        uint8_t v = 8 + i * 10;
        FG_BG_256[j] = ColorRGBA::FromRGB(v,v,v);
    }
}

// FIXME: I am off by one when merging the two strings...
void TerminalController::HandleTerminalData(const uint8_t *buffer, size_t length) {
    VTermParser vtParser;
    auto stripped = vtParser.Parse(buffer, length);
    auto &cmdBuffer = vtParser.LastCmdBuffer();
    auto terminalColors = Editor::Instance().GetTheme()->GetTerminalColor();

    if (stripped.starts_with("gnilk")) {
        int breakme = 1;
        logger->Debug("COLORS HERE!!!!!!!!");
    }

    logger->Debug("Parsing: %s", stripped.c_str());

    size_t idxStartLine = 0;    // after a new line-start in the 'stripped' string
    size_t idxLastLineStart = lastLine->Length() + 1;   // this is the offset of continuation

    size_t idxCmd = 0;
    // This actually works on Linux but not on macOS
    for(size_t idxStr = 0; idxStr < stripped.length(); idxStr++) {
        auto ch = stripped.at(idxStr);
        if (!cmdBuffer.empty()) {
            // Process all commands at this point
            while(cmdBuffer[idxCmd].idxString == idxStr) {
                Line::LineAttrib lAttrib;
                lAttrib.backgroundColor = terminalColors.GetColor("background");
                lAttrib.foregroundColor = terminalColors.GetColor("foreground");

                lAttrib.idxOrigString = idxLastLineStart + (idxStr - idxStartLine) - 1;  // FIXME: Verify on Linux

                switch(cmdBuffer[idxCmd].cmd) {
                    case VTermParser::kAnsiCmd::kSetForegroundColor :
                        // FIXME: Support proper RGB as well
                        if (cmdBuffer[idxCmd].param.size() == 1) {
                            // FIXME: Mapping to RGB - this is color index - we need the 'TerminalTheme'
                            //        There are at maximum 256 colors - the table is fairly common...
                            //        Look at kitty as the source (or iTerm2 or similar)
                            //        Add to theme file..
                            auto idxColor = (cmdBuffer[idxCmd].param[0] & 7) + 8;    // & 7 - I just have 8 colors defined...
                            logger->Debug("SetFGCol=%d @ idx=%d", idxColor, lAttrib.idxOrigString);
                            lAttrib.foregroundColor = terminalColors.GetColor(std::to_string(idxColor)); //FG_BG_256[idxColor];
                            lastLine->Attributes().push_back(lAttrib);
                        }

                    break;
                    case VTermParser::kAnsiCmd::kSetBackgroundColor :
                        if (cmdBuffer[idxCmd].param.size() == 1) {
                            auto idxColor = cmdBuffer[idxCmd].param[0] & 7;    // & 7 - I just have 8 colors defined...
                            logger->Debug("SetBGCol=%d @ idx=%d", idxColor, lAttrib.idxOrigString);
                            lAttrib.backgroundColor = terminalColors.GetColor(std::to_string(idxColor)); //FG_BG_256[idxColor];
                            lastLine->Attributes().push_back(lAttrib);
                        }
                        break;

                    case VTermParser::kAnsiCmd::kSGRReset :
                        // This is done already - in the CTOR of Line::LineAttrib
                        //lAttrib.backgroundColor = Editor::Instance().GetTheme()->GetGlobalColors().GetColor("background");
                        //lAttrib.foregroundColor = Editor::Instance().GetTheme()->GetGlobalColors().GetColor("foreground");
                            logger->Debug("SGR Reset");
                            lAttrib.foregroundColor = terminalColors.GetColor("foreground");
                            lAttrib.backgroundColor = terminalColors.GetColor("background");
                       lastLine->Attributes().push_back(lAttrib);
                        break;
                    case VTermParser::kAnsiCmd::kSetDefaultForegroundColor :
                        //lAttrib.foregroundColor = Editor::Instance().GetTheme()->GetGlobalColors().GetColor("foreground");
                            logger->Debug("SetFGCol=default");
                            lAttrib.foregroundColor = terminalColors.GetColor("foreground");
                        lastLine->Attributes().push_back(lAttrib);
                        break;
                    case VTermParser::kAnsiCmd::kSetDefaultBackgroundColor :
                        //lAttrib.foregroundColor = Editor::Instance().GetTheme()->GetGlobalColors().GetColor("background");
                            logger->Debug("SetBGCol=default");
                            lAttrib.backgroundColor = terminalColors.GetColor("background");

                        lastLine->Attributes().push_back(lAttrib);
                        break;
                }
                //lastLine->Append('*');
                //printf("*");
                //logger->Dbg("at: {}, idxCmd={}, cmd={}", idxStr, idxCmd, static_cast<int>(cmdBuffer[idxCmd].cmd));
                idxCmd++;
            }
        }
        // macos get's \r\n
        if (ch == 0x0a) {
            historyBuffer.push_back(lastLine);
            NewLine();
            // We start a new line at this index...
            idxStartLine = idxStr;
            idxLastLineStart = 0;
        } else if ((ch >= 31) && (ch < 127)) {
            lastLine->Append(ch);
        } else {
            // UTF 8 here..
            // I can probably always use this way...
            std::u32string dummy;
            int nConverted = UnicodeHelper::ConvertUTF8ToUTF32Char(dummy, reinterpret_cast<const uint8_t *>(&stripped[idxStr]), stripped.length() - idxStr);
            if (nConverted > 0) {
                lastLine->Append(dummy);
                idxStr += (nConverted - 1);
            } else {
                // We failed to convert - let's just output something...
                lastLine->Append(U'.');
            }

        }
    }

    Editor::Instance().TriggerUIRedraw();
}


void TerminalController::ParseAndAppend(std::u32string &str) {
    auto asciStr = UnicodeHelper::utf32toascii(str);
    for(auto ch : asciStr) {
        if ((ch >= ' ') && (ch <= 126)) {
            lastLine->Append(ch);
        } else {
            switch (ch) {
                case '\r' :
                    lastLine->Clear();
                    break;
                case '\n' :
                    historyBuffer.push_back(lastLine);
                    NewLine();
                    break;
                default:
                    logger->Debug("Unsupported char: 0x%.2x (%d)",ch,ch);
            }
        }
    }
}

bool TerminalController::HandleKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress) {
    if (DefaultEditLine(inputCursor, inputLine, keyPress)) {
        logger->Debug("InputLine: %s", inputLine->BufferAsUTF8().c_str());
        // The visible cursor is from the lastLine (from shell) to the current input cursor...
        // input cursor is handled by DefaultEditLine..
        cursor.position.x = GetCursorXPos();
        return true;
    }
    return false;
}

bool TerminalController::OnAction(const KeyPressAction &kpAction) {

    switch(kpAction.action) {
        case kAction::kActionLineHome :
            inputCursor.position.x = 0;
            break;
        case kAction::kActionLineEnd :
            inputCursor.position.x = inputLine->Length();
            break;
        case kAction::kActionLineLeft :
            inputCursor.position.x -= 1;
            if (inputCursor.position.x < 0) {
                inputCursor.position.x = 0;
            }
            break;
        case kAction::kActionLineRight :
            inputCursor.position.x++;
            if (inputCursor.position.x > inputLine->Length()) {
                inputCursor.position.x = inputLine->Length();
            }
            break;
// Implement these..
//        case kAction::kActionLineWordLeft :
//        case kAction::kActionLineWordRight :

        default:
            return false;
    }
    //cursor.position.x = lastLine->Length() + inputCursor.position.x;

    Editor::Instance().TriggerUIRedraw();
    return true;
}

int TerminalController::GetCursorXPos() {
    return lastLine->Length() + inputCursor.position.x;
}

void TerminalController::NewLine() {
    auto terminalColors = Editor::Instance().GetTheme()->GetTerminalColor();


    std::lock_guard<std::mutex> guard(lineLock);
    lastLine = std::make_shared<Line>();

    // Setup the line attributes
    Line::LineAttrib lineAttrib = {};
    lineAttrib.idxOrigString = 0;
    lineAttrib.backgroundColor = terminalColors.GetColor("background");
    lineAttrib.foregroundColor = terminalColors.GetColor("foreground");
    //lineAttrib.tokenClass = kLanguageTokenClass::kRegular;

    lastLine->Attributes().push_back(lineAttrib);

}
void TerminalController::CommitLine() {
    auto current = CurrentLine();
    historyBuffer.push_back(current);

    std::u32string cmdLine(inputLine->Buffer());

    inputLine->Clear();
    inputCursor.position.x = 0;
    NewLine();

    // Try execute plugin..
    if (PluginExecutor::ParseAndExecuteWithCmdPrefix(cmdLine)) {
        static auto newline = std::u32string(U"\n");
        shell.SendCmd(newline);
    } else {
        // No luck - send to shell...
        cmdLine+=(U"\n");
        shell.SendCmd(cmdLine);
    }

    Editor::Instance().TriggerUIRedraw();
}

Line::Ref TerminalController::CurrentLine() {
    auto currentLine = Line::Create(lastLine);

    //currentLine->Append(lastLine);
    //currentLine->Attributes().insert(currentLine->Attributes().begin(), lastLine->Attributes().begin(), lastLine->Attributes().end());

    currentLine->Append(inputLine);

    return currentLine;
}

void TerminalController::WriteLine(const std::u32string &str) {

    lastLine->Append(str);
    historyBuffer.push_back(lastLine);
    NewLine();

    Editor::Instance().TriggerUIRedraw();
}

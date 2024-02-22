//
// Created by gnilk on 22.02.2024.
//

#include "TerminalController.h"
#include "Core/Editor.h"
#include "Core/HexDump.h"
#include "Core/Plugins/PluginExecutor.h"

using namespace gedit;

void TerminalController::Begin() {
    logger = gnilk::Logger::GetLogger("TerminalController");
    logger->Debug("Begin");

    RuntimeConfig::Instance().SetOutputConsole(this);


    inputLine = std::make_shared<Line>();
    inputCursor.position.x = 0;

    auto shellStdHandler= [this](std::u32string &str) {

        auto asciStr = UnicodeHelper::utf32toascii(str);
        logger->Debug("Got Data: %s", asciStr.c_str());
        HexDump::ToLog(logger, asciStr.data(), asciStr.size());

        ParseAndAppend(str);
        Editor::Instance().TriggerUIRedraw();
    };
    // Create the first line, we need one to consume data..
    NewLine();

    shell.SetStdoutDelegate(shellStdHandler);
    shell.SetStderrDelegate(shellStdHandler);
    shell.Begin();
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
    std::lock_guard<std::mutex> guard(lineLock);
    lastLine = std::make_shared<Line>();

    // Setup the line attributes
    Line::LineAttrib lineAttrib = {};
    lineAttrib.idxOrigString = 0;
    lineAttrib.tokenClass = kLanguageTokenClass::kRegular;
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
    auto currentLine = Line::Create();

    currentLine->Append(lastLine);
    currentLine->Append(inputLine);

    return currentLine;
}

void TerminalController::WriteLine(const std::u32string &str) {
    // Let's append to current line and commit to history buffer
//    Line::Ref tmp = nullptr;
//
//    if (!lastLine->IsEmpty()) {
//        tmp = Line::Create(lastLine->Buffer());
//    }

    lastLine->Append(str);
    historyBuffer.push_back(lastLine);
    NewLine();

//    if (tmp != nullptr) {
//        lastLine->Append(tmp);
//    }

    Editor::Instance().TriggerUIRedraw();
}

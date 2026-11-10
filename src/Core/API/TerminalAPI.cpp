//
// Created by gnilk on 24.06.24.
//

#include "TerminalAPI.h"
#include "EditorAPI.h"
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/UnicodeHelper.h"
#include "Core/ClipBoard.h"

using namespace gedit;

std::optional<std::u32string> TerminalAPI::GetLastOutput() {
    auto console = Console();
    if (console == nullptr) {
        return std::nullopt;
    }
    return console->GetLastOutputText();
}

DocumentAPI::Ref TerminalAPI::OpenLastOutputAsDocument() {
    auto text = GetLastOutput();
    if (!text.has_value()) {
        return nullptr;
    }
    // Reuse the EditorAPI string->document seam (NewDocumentFromText)
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    if (editorApi == nullptr) {
        return nullptr;
    }
    auto utf8 = UnicodeHelper::utf32to8(text.value());
    return editorApi->NewDocumentFromText("output", utf8.c_str());
}

bool TerminalAPI::CopyLastOutputToClipboard() {
    auto text = GetLastOutput();
    if (!text.has_value()) {
        return false;
    }
    Editor::Instance().GetClipBoard().CopyText(text.value());
    return true;
}

IOutputConsole *TerminalAPI::Console() {
    return RuntimeConfig::Instance().OutputConsole();
}

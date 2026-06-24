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

IOutputConsole *TerminalAPI::Console() {
    return RuntimeConfig::Instance().OutputConsole();
}

std::vector<TerminalAPI::BlockInfo> TerminalAPI::GetBlocks() {
    auto console = Console();
    if (console == nullptr) {
        return {};
    }
    return console->GetBlocks();
}

std::optional<std::u32string> TerminalAPI::GetBlockOutput(uint64_t blockId) {
    auto console = Console();
    if (console == nullptr) {
        return std::nullopt;
    }
    return console->GetBlockOutputText(blockId);
}

std::optional<TerminalAPI::BlockInfo> TerminalAPI::GetLastBlock() {
    auto console = Console();
    if (console == nullptr) {
        return std::nullopt;
    }
    return console->GetLastBlock();
}

std::optional<std::u32string> TerminalAPI::GetSelectedBlock() {
    auto console = Console();
    if (console == nullptr) {
        return std::nullopt;
    }
    return console->GetSelectedBlockText();
}

DocumentAPI::Ref TerminalAPI::OpenBlockAsDocument(uint64_t blockId) {
    auto text = GetBlockOutput(blockId);
    if (!text.has_value()) {
        return nullptr;
    }
    // Reuse the EditorAPI string->document seam (NewDocumentFromText). Name the buffer after the block
    // id so successive opens don't collide.
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    if (editorApi == nullptr) {
        return nullptr;
    }
    std::string name = "block-" + std::to_string(blockId);
    auto utf8 = UnicodeHelper::utf32to8(text.value());
    return editorApi->NewDocumentFromText(name.c_str(), utf8.c_str());
}

bool TerminalAPI::CopyBlockToClipboard(uint64_t blockId) {
    auto text = GetBlockOutput(blockId);
    if (!text.has_value()) {
        return false;
    }
    Editor::Instance().GetClipBoard().CopyText(text.value());
    return true;
}

//
// BuildMessagesModal - the last build's errors and warnings
//

#include <algorithm>
#include <filesystem>

#include "BuildMessagesModal.h"
#include "Core/ActionHelper.h"
#include "Core/Editor.h"
#include "Core/Editor/LineRender.h"
#include "Core/Index/CodeIndex.h"
#include "Core/UI/UIHost.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

BuildMessagesModal::BuildMessagesModal(const std::string &newCaption, std::vector<BuildMessage> newMessages) :
    QueryListModal(newCaption, U"Filter: "), messages(std::move(newMessages)) {
}

BuildMessagesModal::Ptr BuildMessagesModal::Create() {
    auto &buildMessages = Editor::Instance().GetBuildMessages();
    if (!buildMessages.HasRun()) {
        return nullptr;
    }
    auto caption = buildMessages.Command() + " - " + buildMessages.Summary();
    if (buildMessages.IsRunning()) {
        caption += " (running)";
    }
    return std::make_unique<BuildMessagesModal>(caption, buildMessages.Messages());
}

std::vector<size_t> BuildMessagesModal::Filter(const std::vector<BuildMessage> &messages, const std::u32string &query) {
    auto lowerQuery = CodeIndex::LowerCase(query);
    std::vector<size_t> rows;
    for (auto idxMessage : BuildMessages::Order(messages)) {
        if (query.empty() || (CodeIndex::LowerCase(RowText(messages[idxMessage])).find(lowerQuery) != std::u32string::npos)) {
            rows.push_back(idxMessage);
        }
    }
    return rows;
}

std::u32string BuildMessagesModal::RowText(const BuildMessage &message) {
    return UnicodeHelper::utf8to32(BuildMessages::RowText(message));
}

std::vector<std::u32string> BuildMessagesModal::OnQuery(const std::u32string &query) {
    rowMessages = Filter(messages, query);
    std::vector<std::u32string> listRows;
    for (auto idxMessage : rowMessages) {
        listRows.push_back(RowText(messages[idxMessage]));
    }
    return listRows;
}

// The folder, as printed - which of two 'util.h' it is
std::u32string BuildMessagesModal::OnRowDetail(int idxRow) {
    auto folder = std::filesystem::path(messages[rowMessages[(size_t)idxRow]].path).parent_path().string();
    return UnicodeHelper::utf8to32(folder);
}

void BuildMessagesModal::DrawRowLine(DrawContext &dc, int x, int y, int width, int idxRow, int idxLine) {
    auto idxMessage = rowMessages[(size_t)idxRow];
    auto it = codeLines.find(idxMessage);
    if (it == codeLines.end()) {
        auto &uiColors = UIHost::Instance().GetUIColors();
        auto background = uiColors.HasColor("modal_background") ? uiColors["modal_background"] : uiColors["background"];
        it = codeLines.emplace(idxMessage, ActionHelper::ListedSourceLine(Editor::Instance().GetBuildMessages().ResolvePath(messages[idxMessage]),
                                                                            messages[idxMessage].line, background)).first;
    }
    auto xCode = x + BuildMessages::CODE_INDENT;
    if ((it->second == nullptr) || (xCode >= x + width)) {
        return;
    }
    LineRender(dc).DrawLine(xCode, y, it->second->CopyOfRange(0, (size_t)(x + width - xCode)));
}

void BuildMessagesModal::OnCommit(int idxRow) {
    auto position = Editor::Instance().GetBuildMessages().Select(rowMessages[(size_t)idxRow]);
    if (position.has_value()) {
        ActionHelper::GoToBuildMessage(position.value());
    }
}

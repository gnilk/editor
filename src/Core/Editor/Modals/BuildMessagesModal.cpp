//
// BuildMessagesModal - the last build's errors and warnings
//

#include <algorithm>
#include <filesystem>

#include "BuildMessagesModal.h"
#include "Core/ActionHelper.h"
#include "Core/Editor.h"
#include "Core/Index/CodeIndex.h"
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
    std::string place = std::filesystem::path(message.path).filename().string();
    if (message.line > 0) {
        place += ":" + std::to_string(message.line);
    }
    if (message.column > 0) {
        place += ":" + std::to_string(message.column);
    }
    auto severity = message.IsError() ? "error    " : "warning  ";
    return UnicodeHelper::utf8to32(severity + place + "  " + message.message);
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

void BuildMessagesModal::OnCommit(int idxRow) {
    auto position = Editor::Instance().GetBuildMessages().Select(rowMessages[(size_t)idxRow]);
    if (position.has_value()) {
        ActionHelper::GoToBuildMessage(position.value());
    }
}

//
// Created by gnilk on 05.06.2026.
//

#include "TerminalCmdHistory.h"
#include "UnicodeHelper.h"
#include <cstring>
#include <fstream>

using namespace gedit;

void TerminalCmdHistory::Push(const std::u32string &line) {
    if (line.empty()) {
        return;
    }
    if (!entries.empty() && entries.back() == line) {
        return;
    }
    entries.push_back(line);
    if (entries.size() > MAX_ENTRIES) {
        entries.erase(entries.begin());
    }
    navIndex = -1;
}

std::optional<std::u32string> TerminalCmdHistory::NavigateUp() {
    if (entries.empty()) {
        return std::nullopt;
    }
    if (navIndex == -1) {
        navIndex = (int)entries.size() - 1;
    } else if (navIndex > 0) {
        navIndex--;
    }
    return entries[navIndex];
}

std::optional<std::u32string> TerminalCmdHistory::NavigateDown() {
    if (navIndex == -1) {
        return std::nullopt;
    }
    navIndex++;
    if (navIndex >= (int)entries.size()) {
        navIndex = -1;
        return std::nullopt;
    }
    return entries[navIndex];
}

void TerminalCmdHistory::ResetNavigation() {
    navIndex = -1;
}

bool TerminalCmdHistory::Load(AssetLoaderBase::Asset::Ref asset) {
    if (asset == nullptr) {
        return false;
    }
    const char *text = asset->GetPtrAs<const char *>();
    Parse(text, text + asset->GetSize());
    return true;
}

bool TerminalCmdHistory::Load(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Parse(content.data(), content.data() + content.size());
    return true;
}

bool TerminalCmdHistory::Save(const std::filesystem::path &path) const {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    for (const auto &entry : entries) {
        file << UnicodeHelper::utf32to8(entry) << '\n';
    }
    return true;
}

void TerminalCmdHistory::Parse(const char *text, const char *end) {
    entries.clear();
    while (text < end) {
        const char *newline = static_cast<const char *>(memchr(text, '\n', end - text));
        size_t len = (newline != nullptr) ? (size_t)(newline - text) : (size_t)(end - text);
        if (len > 0) {
            entries.push_back(UnicodeHelper::utf8to32(std::string(text, len)));
        }
        if (newline == nullptr) {
            break;
        }
        text = newline + 1;
    }
}

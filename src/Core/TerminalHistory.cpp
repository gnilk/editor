//
// Created by gnilk on 05.06.2026.
//

#include "TerminalHistory.h"
#include "UnicodeHelper.h"
#include <cstring>
#include <fstream>

using namespace gedit;

void TerminalHistory::Push(const std::u32string &line) {
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

std::optional<std::u32string> TerminalHistory::NavigateUp() {
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

std::optional<std::u32string> TerminalHistory::NavigateDown() {
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

void TerminalHistory::ResetNavigation() {
    navIndex = -1;
}

bool TerminalHistory::Load(AssetLoaderBase::Asset::Ref asset) {
    if (asset == nullptr) {
        return false;
    }
    entries.clear();
    const char *text = asset->GetPtrAs<const char *>();
    const char *end = text + asset->GetSize();
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
    return true;
}

bool TerminalHistory::Save(const std::filesystem::path &path) const {
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

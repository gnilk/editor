//
// Created by gnilk on 05.06.2026.
//

#ifndef GOATEDIT_TERMINALHISTORY_H
#define GOATEDIT_TERMINALHISTORY_H

#include <vector>
#include <string>
#include <optional>
#include <filesystem>
#include "AssetLoaderBase.h"

namespace gedit {
    class TerminalHistory {
    public:
        static constexpr size_t MAX_ENTRIES = 1000;

        // Push a command; skips empty lines and consecutive duplicates.
        void Push(const std::u32string &line);

        // Walk history. Up returns the next older entry; Down returns the next newer
        // entry, or an empty optional when back at live (unnavigated) input.
        std::optional<std::u32string> NavigateUp();
        std::optional<std::u32string> NavigateDown();

        // Reset to live-input position (call after committing a line).
        void ResetNavigation();

        bool Load(AssetLoaderBase::Asset::Ref asset);
        bool Save(const std::filesystem::path &path) const;

    private:
        std::vector<std::u32string> entries;
        int navIndex = -1;  // -1 = live input; >= 0 = index into entries (0 = oldest)
    };
}

#endif //GOATEDIT_TERMINALHISTORY_H

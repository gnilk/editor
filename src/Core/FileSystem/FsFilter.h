//
// Created by gnilk on 17.06.26.
//
// FsFilter — the name/glob exclusion shared by the folder scanner and (when re-enabled) the folder
// monitor, so a path one prunes the other also ignores (docs/folder-scanner.md §6). Patterns match an
// entry's FILENAME (last path component) via Core/Glob.h — e.g. ".git", "cmake-build-debug",
// "cmake-build-*". Plain data in, plain bool out: no editor types, no singleton.
//
#ifndef GOATEDIT_FSFILTER_H
#define GOATEDIT_FSFILTER_H

#include <filesystem>
#include <string>
#include <vector>

namespace gedit {

    class FsFilter {
    public:
        FsFilter() = default;
        explicit FsFilter(std::vector<std::string> patterns);

        bool IsEmpty() const;
        // True if the entry's filename matches any exclude pattern (=> prune: no event, no descent).
        bool IsExcluded(const std::filesystem::path &path) const;

    private:
        std::vector<std::string> patterns;
    };
}

#endif //GOATEDIT_FSFILTER_H

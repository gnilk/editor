//
// Created by gnilk on 17.06.26.
//
#include "FsFilter.h"
#include "Core/Glob.h"

using namespace gedit;
namespace fs = std::filesystem;

FsFilter::FsFilter(std::vector<std::string> patterns) : patterns(std::move(patterns)) {
}

bool FsFilter::IsEmpty() const {
    return patterns.empty();
}

bool FsFilter::IsExcluded(const fs::path &path) const {
    return IsNameExcluded(path.filename().string());
}

bool FsFilter::IsExcludedRelative(const fs::path &relPath) const {
    for (const auto &component : relPath) {
        const auto name = component.string();
        if (name.empty() || (name == ".")) {
            continue;
        }
        if (IsNameExcluded(name)) {
            return true;
        }
    }
    return false;
}

bool FsFilter::IsNameExcluded(const std::string &name) const {
    for (const auto &pattern : patterns) {
        if (Glob::Match(pattern, name) == Glob::kMatch::Match) {
            return true;
        }
    }
    return false;
}

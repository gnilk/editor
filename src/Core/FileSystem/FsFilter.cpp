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
    const auto name = path.filename().string();
    for (const auto &pattern : patterns) {
        if (Glob::Match(pattern, name) == Glob::kMatch::Match) {
            return true;
        }
    }
    return false;
}

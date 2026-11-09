//
// TypeToFind - see TypeToFind.h
//
#include "TypeToFind.h"

#include <cwctype>

using namespace gedit;

const std::u32string &TypeToFind::Type(char32_t ch, Clock::time_point now) {
    if (prefix.empty() || ((now - timeLastTyped) >= pause)) {
        prefix.clear();
    }
    prefix += ch;
    timeLastTyped = now;
    return prefix;
}

std::optional<size_t> TypeToFind::FindNext(const std::vector<std::u32string> &names, size_t idxCurrent,
                                           const std::u32string &prefix) {
    if (names.empty() || prefix.empty()) {
        return std::nullopt;
    }
    idxCurrent = (idxCurrent < names.size()) ? idxCurrent : 0;
    // One letter: the next one with it, not the row we are on
    if (prefix.size() == 1) {
        return FindFrom(names, idxCurrent + 1, prefix);
    }
    auto found = FindFrom(names, idxCurrent, prefix);
    if (!found.has_value() && IsOneLetterRepeated(prefix)) {
        return FindFrom(names, idxCurrent + 1, prefix.substr(0, 1));
    }
    return found;
}

// From 'idxStart' to the end, then from the top - every row once
std::optional<size_t> TypeToFind::FindFrom(const std::vector<std::u32string> &names, size_t idxStart,
                                           const std::u32string &prefix) {
    for (size_t i = 0; i < names.size(); i++) {
        auto idx = (idxStart + i) % names.size();
        if (StartsWith(names[idx], prefix)) {
            return idx;
        }
    }
    return std::nullopt;
}

bool TypeToFind::StartsWith(const std::u32string &name, const std::u32string &prefix) {
    if (name.size() < prefix.size()) {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); i++) {
        if (std::towlower((wint_t)name[i]) != std::towlower((wint_t)prefix[i])) {
            return false;
        }
    }
    return true;
}

bool TypeToFind::IsOneLetterRepeated(const std::u32string &prefix) {
    for (auto ch : prefix) {
        if (std::towlower((wint_t)ch) != std::towlower((wint_t)prefix[0])) {
            return false;
        }
    }
    return true;
}

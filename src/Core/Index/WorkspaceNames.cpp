//
// Name -> token class across the workspace - see WorkspaceNames.h
//

#include "WorkspaceNames.h"

using namespace gedit;

static bool IsBlank(char32_t ch) {
    return (ch == U' ') || (ch == U'\t');
}

static bool IsFollowedByCall(const std::u32string &text, size_t idxEnd) {
    while ((idxEnd < text.size()) && IsBlank(text[idxEnd])) {
        idxEnd++;
    }
    return (idxEnd < text.size()) && (text[idxEnd] == U'(');
}

static bool EndsWithWord(const std::u32string &text, size_t idxEnd, const std::u32string &word) {
    return (idxEnd >= word.size()) && (text.compare(idxEnd - word.size(), word.size(), word) == 0);
}

// 'auto f() -> Foo', '[]() const -> Foo': the '->' is spaced off a parameter list, or a qualifier behind one. A call
// chain 'f()->x' isn't.
static bool IsTrailingReturn(const std::u32string &text, size_t idxArrow) {
    auto idxEnd = idxArrow;
    while ((idxEnd > 0) && IsBlank(text[idxEnd - 1])) {
        idxEnd--;
    }
    if ((idxEnd == idxArrow) || (idxEnd == 0)) {
        return false;
    }
    return (text[idxEnd - 1] == U')') || EndsWithWord(text, idxEnd, U"const") || EndsWithWord(text, idxEnd, U"noexcept") ||
           EndsWithWord(text, idxEnd, U"mutable") || EndsWithWord(text, idxEnd, U"override");
}

static bool IsReachedThroughObject(const std::u32string &text, size_t idxName) {
    while ((idxName > 0) && IsBlank(text[idxName - 1])) {
        idxName--;
    }
    if (idxName == 0) {
        return false;
    }
    if (text[idxName - 1] == U'.') {
        // 'Args... args'
        return (idxName < 2) || (text[idxName - 2] != U'.');
    }
    if ((idxName < 2) || (text[idxName - 1] != U'>') || (text[idxName - 2] != U'-')) {
        return false;
    }
    return !IsTrailingReturn(text, idxName - 2);
}

// 'current', 'ptr', 'other': types and enum values in some file, and locals in every other one. 'kFoo' isn't.
static bool CouldBeLocal(const std::u32string &name) {
    bool isLower = (name[0] >= U'a') && (name[0] <= U'z');
    bool isHungarian = (name.size() > 1) && (name[1] >= U'A') && (name[1] <= U'Z');
    return isLower && !isHungarian;
}

// A local can't stand here: after '::', or before a name, a call, '::', template arguments or a declarator
static bool IsInTypePosition(const std::u32string &text, size_t idxName, size_t idxEnd) {
    if ((idxName > 1) && (text[idxName - 1] == U':') && (text[idxName - 2] == U':')) {
        return true;
    }
    while ((idxEnd < text.size()) && IsBlank(text[idxEnd])) {
        idxEnd++;
    }
    if (idxEnd == text.size()) {
        return false;
    }
    auto next = text[idxEnd];
    bool isNameStart = ((next >= U'a') && (next <= U'z')) || ((next >= U'A') && (next <= U'Z')) || (next == U'_');
    if (isNameStart || (next == U'(') || (next == U'<') || (next == U'*') || (next == U'&')) {
        return true;
    }
    return (next == U':') && ((idxEnd + 1) < text.size()) && (text[idxEnd + 1] == U':');
}

// Before '<' it can't say: 'a.get<int>()' or 'a.count < 3'
kLanguageTokenClass WorkspaceNames::ClassFromSyntax(const std::u32string &text, size_t idxName, size_t idxEnd) {
    if (!IsReachedThroughObject(text, idxName)) {
        return kLanguageTokenClass::kRegular;
    }
    if (IsFollowedByCall(text, idxEnd)) {
        return kLanguageTokenClass::kFunction;
    }
    while ((idxEnd < text.size()) && IsBlank(text[idxEnd])) {
        idxEnd++;
    }
    bool isUndecided = (idxEnd < text.size()) && (text[idxEnd] == U'<');
    return isUndecided ? kLanguageTokenClass::kRegular : kLanguageTokenClass::kMember;
}

void WorkspaceNames::Add(const std::u32string &name, kLanguageTokenClass tokenClass) {
    names[name] = tokenClass;
}

kLanguageTokenClass WorkspaceNames::Lookup(const std::u32string &name) const {
    auto it = names.find(name);
    return (it == names.end()) ? kLanguageTokenClass::kRegular : it->second;
}

kLanguageTokenClass WorkspaceNames::LookupAt(const std::u32string &text, size_t idxName, const std::u32string &name) const {
    return ClassInContext(text, idxName, name, Lookup(name));
}

kLanguageTokenClass WorkspaceNames::ClassInContext(const std::u32string &text, size_t idxName, const std::u32string &name, kLanguageTokenClass tokenClass) {
    if (tokenClass == kLanguageTokenClass::kRegular) {
        return tokenClass;
    }
    if ((tokenClass == kLanguageTokenClass::kFunction) || (tokenClass == kLanguageTokenClass::kLibraryFunction)) {
        return IsFollowedByCall(text, idxName + name.size()) ? tokenClass : kLanguageTokenClass::kRegular;
    }
    if (tokenClass == kLanguageTokenClass::kMember) {
        return IsReachedThroughObject(text, idxName) ? tokenClass : kLanguageTokenClass::kRegular;
    }
    if (CouldBeLocal(name) && !IsInTypePosition(text, idxName, idxName + name.size())) {
        return kLanguageTokenClass::kRegular;
    }
    return tokenClass;
}

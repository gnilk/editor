//
// A C/C++ type as written - see TypeRef.h
//

#include <algorithm>

#include "TypeRef.h"

using namespace gedit;

namespace {
    // Ownership idioms (docs/code-index.md §5.4) - tables in code for now, config later (step 7)
    const std::vector<std::u32string> kOwningPointers = { U"unique_ptr", U"auto_ptr" };
    const std::vector<std::u32string> kSharingPointers = { U"shared_ptr", U"weak_ptr" };
    const std::vector<std::u32string> kContainers = {
        U"vector", U"list", U"deque", U"array", U"set", U"unordered_set", U"multiset", U"map", U"unordered_map",
        U"multimap", U"optional", U"stack", U"queue", U"priority_queue", U"forward_list", U"span",
    };
    const std::vector<std::u32string> kMaps = { U"map", U"unordered_map", U"multimap", U"unordered_multimap" };
    // Project idioms: 'T::Ref' is a shared_ptr<T>, 'T::Ptr' a unique_ptr<T> (GoatEdit)
    const std::u32string kRefAlias = U"Ref";
    const std::u32string kPtrAlias = U"Ptr";

    // Words before or in a type that don't name it
    const std::vector<std::u32string> kTypeQualifiers = {
        U"const", U"volatile", U"static", U"mutable", U"inline", U"constexpr", U"typename", U"struct", U"class",
        U"enum", U"union", U"extern", U"thread_local", U"register", U"unsigned", U"signed",
    };

    bool Contains(const std::vector<std::u32string> &list, const std::u32string &text) {
        return std::find(list.begin(), list.end(), text) != list.end();
    }

    bool IsWordChar(char32_t ch) {
        return ((ch >= U'a') && (ch <= U'z')) || ((ch >= U'A') && (ch <= U'Z')) || ((ch >= U'0') && (ch <= U'9')) ||
               (ch == U'_') || (ch > 127);
    }

    std::u32string LastComponent(const std::u32string &qualifiedName) {
        auto idx = qualifiedName.rfind(U"::");
        return (idx == std::u32string::npos) ? qualifiedName : qualifiedName.substr(idx + 2);
    }

    std::u32string Qualifier(const std::u32string &qualifiedName) {
        auto idx = qualifiedName.rfind(U"::");
        return (idx == std::u32string::npos) ? std::u32string() : qualifiedName.substr(0, idx);
    }

    TypeRef WithoutIndirection(TypeRef type) {
        type.numPointers = 0;
        type.isReference = false;
        return type;
    }
}

// Recursive descent: name [ '<' args '>' ] { '*' | '&' | '&&' | const }
TypeRef TypeRef::Parse(const std::vector<Token> &tokens, size_t &idx) {
    TypeRef type;
    while ((idx < tokens.size()) && Contains(kTypeQualifiers, tokens[idx].text)) {
        idx++;
    }
    while (idx < tokens.size()) {
        auto &text = tokens[idx].text;
        if (text == U"::") {
            type.name += text;
            idx++;
            continue;
        }
        if (text.empty() || !IsWordChar(text[0]) || ((text[0] >= U'0') && (text[0] <= U'9'))) {
            break;
        }
        // Two words in a row ('unsigned long') - keep the last; builtin either way
        if (!type.name.empty() && (type.name.back() != U':')) {
            type.name.clear();
        }
        type.name += text;
        type.isBuiltin = type.isBuiltin || tokens[idx].isBuiltin;
        idx++;
    }
    if ((idx < tokens.size()) && (tokens[idx].text == U"<")) {
        idx++;
        int depth = 0;
        while (idx < tokens.size()) {
            if (tokens[idx].text == U">") {
                idx++;
                break;
            }
            if (tokens[idx].text == U",") {
                idx++;
                continue;
            }
            auto before = idx;
            type.args.push_back(Parse(tokens, idx));
            if (idx == before) {
                // Something we don't understand (a value argument) - skip it, and what it opens
                type.args.pop_back();
                depth += (tokens[idx].text == U"(") ? 1 : 0;
                idx++;
                while ((depth > 0) && (idx < tokens.size())) {
                    depth += (tokens[idx].text == U"(") ? 1 : (tokens[idx].text == U")") ? -1 : 0;
                    idx++;
                }
            } else if (type.args.back().name.empty()) {
                type.args.pop_back();
            }
        }
        // 'Foo<T>::Ref'
        if ((idx + 1 < tokens.size()) && (tokens[idx].text == U"::")) {
            idx++;
            auto nested = Parse(tokens, idx);
            type.name += U"::" + nested.name;
            type.args.clear();
            type.numPointers = nested.numPointers;
            type.isReference = nested.isReference;
            return type;
        }
    }
    while (idx < tokens.size()) {
        auto &text = tokens[idx].text;
        if (text == U"*") {
            type.numPointers++;
        } else if ((text == U"&") || (text == U"&&")) {
            type.isReference = true;
        } else if ((text != U"const") && (text != U"volatile")) {
            break;
        }
        idx++;
    }
    return type;
}

TypeRef TypeRef::FromSpelling(const std::u32string &spelling) {
    std::vector<Token> tokens;
    size_t i = 0;
    while (i < spelling.size()) {
        auto ch = spelling[i];
        if (IsWordChar(ch)) {
            auto idxStart = i;
            while ((i < spelling.size()) && IsWordChar(spelling[i])) {
                i++;
            }
            tokens.push_back({spelling.substr(idxStart, i - idxStart)});
        } else if ((ch == U':') && (i + 1 < spelling.size()) && (spelling[i + 1] == U':')) {
            tokens.push_back({U"::"});
            i += 2;
        } else if (ch == U' ') {
            i++;
        } else {
            tokens.push_back({std::u32string(1, ch)});
            i++;
        }
    }
    size_t idx = 0;
    return Parse(tokens, idx);
}

std::u32string TypeRef::Spelling() const {
    auto spelling = name;
    if (!args.empty()) {
        spelling += U"<";
        for (size_t i = 0; i < args.size(); i++) {
            spelling += (i == 0) ? args[i].Spelling() : U"," + args[i].Spelling();
        }
        spelling += U">";
    }
    spelling.append((size_t)numPointers, U'*');
    if (isReference) {
        spelling += U"&";
    }
    return spelling;
}

TypeRef TypeRef::Accessed() const {
    auto last = LastComponent(name);
    auto qualifier = Qualifier(name);
    if ((Contains(kOwningPointers, last) || Contains(kSharingPointers, last)) && !args.empty()) {
        return WithoutIndirection(args[0]);
    }
    if (((last == kRefAlias) || (last == kPtrAlias)) && !qualifier.empty()) {
        TypeRef target;
        target.name = qualifier;
        return target;
    }
    return WithoutIndirection(*this);
}

std::optional<TypeRef> TypeRef::Element() const {
    if (numPointers > 0) {
        auto target = *this;
        target.numPointers--;
        target.isReference = false;
        return target;
    }
    if (Contains(kContainers, LastComponent(name)) && !args.empty()) {
        return args.back();
    }
    return {};
}

std::optional<TypeRef> TypeRef::RangeElement() const {
    if ((numPointers == 0) && Contains(kMaps, LastComponent(name))) {
        return {};
    }
    return Element();
}

std::pair<std::u32string, kOwnership> TypeRef::Held(bool isExtraPointer) const {
    bool isPointer = isExtraPointer || (numPointers > 0) || isReference;
    auto last = LastComponent(name);
    auto qualifier = Qualifier(name);
    if (isBuiltin || name.empty()) {
        return {};
    }
    if (Contains(kOwningPointers, last) && !args.empty()) {
        auto [target, ownership] = args[0].Held(false);
        return {target, isPointer ? kOwnership::kBorrows : kOwnership::kOwns};
    }
    if (Contains(kSharingPointers, last) && !args.empty()) {
        auto [target, ownership] = args[0].Held(false);
        return {target, isPointer ? kOwnership::kBorrows : kOwnership::kShares};
    }
    if ((last == kRefAlias) && !qualifier.empty()) {
        return {qualifier, isPointer ? kOwnership::kBorrows : kOwnership::kShares};
    }
    if ((last == kPtrAlias) && !qualifier.empty()) {
        return {qualifier, isPointer ? kOwnership::kBorrows : kOwnership::kOwns};
    }
    if (Contains(kContainers, last) && !args.empty()) {
        // The element (for a map: the value)
        auto [target, ownership] = args.back().Held(false);
        return {target, isPointer ? kOwnership::kBorrows : ownership};
    }
    // Other library types (std::string, std::mutex, std::function) aren't structure of this code base
    if (name.rfind(U"std::", 0) == 0) {
        return {};
    }
    return {name, isPointer ? kOwnership::kBorrows : kOwnership::kOwns};
}

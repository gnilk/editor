//
// Names from the standard library and the shipped lists - see LibraryNames.h
//

#include <algorithm>
#include <optional>

#include <yaml-cpp/yaml.h>

#include "LibraryNames.h"
#include "WorkspaceNames.h"
#include "logger.h"
#include "Core/RuntimeConfig.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

enum class kQualifier {
    kNone,      // 'x'
    kGlobal,    // '::x'
    kStd,       // 'std::x', '::std::x', 'std::chrono::x'
    kOther,     // 'Foo::x', 'a<b>::x'
};

static bool IsIdentifierChar(char32_t ch) {
    return ((ch >= U'a') && (ch <= U'z')) || ((ch >= U'A') && (ch <= U'Z')) || ((ch >= U'0') && (ch <= U'9')) || (ch == U'_');
}

static bool IsPrecededByScope(const std::u32string &text, size_t idx) {
    return (idx > 1) && (text[idx - 1] == U':') && (text[idx - 2] == U':');
}

// Walks the names before 'idxName' to the outermost one
static kQualifier QualifierOf(const std::u32string &text, size_t idxName) {
    if (!IsPrecededByScope(text, idxName)) {
        return kQualifier::kNone;
    }
    size_t idx = idxName;
    size_t idxOuter = idxName;
    while (IsPrecededByScope(text, idx)) {
        idx -= 2;
        auto idxEnd = idx;
        while ((idx > 0) && IsIdentifierChar(text[idx - 1])) {
            idx--;
        }
        if (idx == idxEnd) {
            if ((idx > 0) && (text[idx - 1] == U'>')) {
                return kQualifier::kOther;
            }
            break;
        }
        idxOuter = idx;
    }
    if (idxOuter == idxName) {
        return kQualifier::kGlobal;
    }
    bool isStd = (text.compare(idxOuter, 3, U"std") == 0) && (text[idxOuter + 3] == U':');
    return isStd ? kQualifier::kStd : kQualifier::kOther;
}

static std::optional<kLanguageTokenClass> TokenClassForKind(const std::string &kind) {
    if (kind == "function") {
        return kLanguageTokenClass::kLibraryFunction;
    }
    if (kind == "type") {
        return kLanguageTokenClass::kLibraryType;
    }
    if (kind == "macro") {
        return kLanguageTokenClass::kMacro;
    }
    if (kind == "namespace") {
        return kLanguageTokenClass::kNamespace;
    }
    if (kind == "constant") {
        return kLanguageTokenClass::kEnumValue;
    }
    return {};
}

LibraryNames::Ref LibraryNames::LoadShipped() {
    auto logger = gnilk::Logger::GetLogger("LibraryNames");
    auto libraryNames = std::make_shared<LibraryNames>();
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    for (auto fileName : {"Libraries/c_std.yml", "Libraries/cpp_std.yml"}) {
        auto asset = assetLoader.LoadTextAsset(fileName);
        if ((asset == nullptr) || !libraryNames->Add(asset->GetPtrAs<char *>())) {
            logger->Error("Can't load library names from '%s'", fileName);
        }
    }
    return libraryNames;
}

bool LibraryNames::IsQualifiedStd(const std::u32string &text, size_t idxName) {
    return QualifierOf(text, idxName) == kQualifier::kStd;
}

bool LibraryNames::Add(const std::string &yaml) {
    auto added = names;
    try {
        auto root = YAML::Load(yaml);
        if (!root["entries"].IsSequence()) {
            return false;
        }
        for (const auto &group : root["entries"]) {
            auto tokenClass = TokenClassForKind(group["kind"].as<std::string>());
            if (!tokenClass.has_value() || !group["names"].IsSequence()) {
                return false;
            }
            auto scope = UnicodeHelper::utf8to32(group["scope"].as<std::string>(""));
            for (const auto &item : group["names"]) {
                // A name, or a name holding its members
                auto name = item.IsMap() ? item.begin()->first.as<std::string>() : item.as<std::string>();
                auto &entry = added[UnicodeHelper::utf8to32(name)];
                if (entry.scopes.empty()) {
                    entry.tokenClass = *tokenClass;
                }
                entry.scopes.push_back(scope);
            }
        }
    } catch (const YAML::Exception &) {
        return false;
    }
    names = std::move(added);
    return true;
}

kLanguageTokenClass LibraryNames::LookupQualified(const std::u32string &text, size_t idxName, const std::u32string &name) const {
    auto it = names.find(name);
    if (it != names.end()) {
        return it->second.tokenClass;
    }
    auto idxEnd = idxName + name.size();
    if ((idxEnd < text.size()) && (text[idxEnd] == U'(')) {
        return kLanguageTokenClass::kLibraryFunction;
    }
    bool isScope = ((idxEnd + 1) < text.size()) && (text[idxEnd] == U':') && (text[idxEnd + 1] == U':');
    return isScope ? kLanguageTokenClass::kNamespace : kLanguageTokenClass::kLibraryType;
}

kLanguageTokenClass LibraryNames::LookupBare(const std::u32string &text, size_t idxName, const std::u32string &name, const SemanticNames &fileNames) const {
    auto it = names.find(name);
    if (it == names.end()) {
        return kLanguageTokenClass::kRegular;
    }
    auto qualifier = QualifierOf(text, idxName);
    if (qualifier == kQualifier::kOther) {
        return kLanguageTokenClass::kRegular;
    }
    auto &entry = it->second;
    bool isVisible = std::any_of(entry.scopes.begin(), entry.scopes.end(), [&](const std::u32string &scope) {
        return scope.empty() || fileNames.IsUsingNamespace(scope) || fileNames.IsUsing(scope + U"::" + name);
    });
    if (!isVisible) {
        return kLanguageTokenClass::kRegular;
    }
    // '::tolower' can't be a local, called or not
    return (qualifier == kQualifier::kGlobal) ? entry.tokenClass : WorkspaceNames::ClassInContext(text, idxName, name, entry.tokenClass);
}

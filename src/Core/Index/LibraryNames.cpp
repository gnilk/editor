//
// Names from the standard library and the shipped lists - see LibraryNames.h
//

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>

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

static bool IsPrecededByScope(const std::u32string_view &text, size_t idx) {
    return (idx > 1) && (text[idx - 1] == U':') && (text[idx - 2] == U':');
}

struct Qualification {
    kQualifier kind = kQualifier::kNone;
    std::u32string_view written;    // 'std::chrono' - empty when template arguments hide it
};

// Walks the names before 'idxName' to the outermost one
static Qualification QualifierOf(const std::u32string_view &text, size_t idxName) {
    if (!IsPrecededByScope(text, idxName)) {
        return {};
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
                return {kQualifier::kOther, {}};
            }
            break;
        }
        idxOuter = idx;
    }
    if (idxOuter == idxName) {
        return {kQualifier::kGlobal, {}};
    }
    auto written = text.substr(idxOuter, idxName - 2 - idxOuter);
    bool isStd = (written == U"std") || written.starts_with(U"std::");
    return {isStd ? kQualifier::kStd : kQualifier::kOther, written};
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

LibraryNames::Ref LibraryNames::Load(const std::optional<std::filesystem::path> &projectDir) {
    auto logger = gnilk::Logger::GetLogger("LibraryNames");
    auto libraryNames = std::make_shared<LibraryNames>();
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    for (auto fileName : {"Libraries/c_std.yml", "Libraries/cpp_std.yml"}) {
        auto asset = assetLoader.LoadTextAsset(fileName, AssetLoaderBase::kLocationType::kSystem);
        if ((asset == nullptr) || !libraryNames->Add(asset->GetPtrAs<char *>())) {
            logger->Error("Can't load library names from '%s'", fileName);
        }
    }
    if (!projectDir.has_value()) {
        return libraryNames;
    }
    std::error_code error;
    std::vector<std::filesystem::path> projectLists;
    for (auto &file : std::filesystem::directory_iterator(*projectDir / "libraries", error)) {
        if (file.path().extension() == ".yml") {
            projectLists.push_back(file.path());
        }
    }
    // The directory has no order of its own, and the first list to name something decides its class
    std::sort(projectLists.begin(), projectLists.end());
    for (auto &path : projectLists) {
        std::ifstream stream(path);
        std::stringstream yaml;
        yaml << stream.rdbuf();
        if (!libraryNames->Add(yaml.str())) {
            logger->Error("Can't load library names from '%s'", path.c_str());
        }
    }
    return libraryNames;
}

bool LibraryNames::IsQualifiedStd(const std::u32string &text, size_t idxName) {
    return QualifierOf(text, idxName).kind == kQualifier::kStd;
}

bool LibraryNames::Add(const std::string &yaml) {
    auto added = names;
    Headers headers;
    try {
        auto root = YAML::Load(yaml);
        if (!root["entries"].IsSequence()) {
            return false;
        }
        if (root["headers"].IsDefined()) {
            if (!root["headers"].IsSequence()) {
                return false;
            }
            for (const auto &header : root["headers"]) {
                headers.push_back(UnicodeHelper::utf8to32(header.as<std::string>()));
            }
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
                added[UnicodeHelper::utf8to32(name)].push_back({*tokenClass, scope, lists.size()});
            }
        }
    } catch (const YAML::Exception &) {
        return false;
    }
    names = std::move(added);
    lists.push_back(std::move(headers));
    return true;
}

bool LibraryNames::IsApplying(const Headers &headers, const SemanticNames &fileNames) {
    return headers.empty() || std::any_of(headers.begin(), headers.end(), [&fileNames](const std::u32string &header) {
        return fileNames.IsIncluding(header);
    });
}

kLanguageTokenClass LibraryNames::LookupQualified(const std::u32string &text, size_t idxName, const std::u32string &name) const {
    auto it = names.find(name);
    if (it != names.end()) {
        return it->second.front().tokenClass;
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
    auto qualification = QualifierOf(text, idxName);
    for (auto &origin : it->second) {
        if (!IsApplying(lists[origin.idxList], fileNames)) {
            continue;
        }
        // 'fmt::format' says what it is. 'Foo::remove' is Foo's, whatever <cstdio> says.
        if (qualification.kind == kQualifier::kOther) {
            if (!origin.scope.empty() && (qualification.written == origin.scope)) {
                return origin.tokenClass;
            }
            continue;
        }
        if (!origin.scope.empty() && !fileNames.IsUsingNamespace(origin.scope) && !fileNames.IsUsing(origin.scope + U"::" + name)) {
            continue;
        }
        // '::tolower' can't be a local, called or not
        bool isGlobal = qualification.kind == kQualifier::kGlobal;
        return isGlobal ? origin.tokenClass : WorkspaceNames::ClassInContext(text, idxName, name, origin.tokenClass);
    }
    return kLanguageTokenClass::kRegular;
}

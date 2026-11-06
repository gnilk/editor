//
// Store and query - see CodeIndex.h
//

#include <algorithm>
#include <filesystem>
#include <mutex>

#include "CodeIndex.h"
#include "SemanticNames.h"

using namespace gedit;

static char32_t LowerChar(char32_t ch) {
    return ((ch >= U'A') && (ch <= U'Z')) ? (ch - U'A' + U'a') : ch;
}

static std::u32string Lower(const std::u32string_view &text) {
    std::u32string lower(text);
    std::transform(lower.begin(), lower.end(), lower.begin(), LowerChar);
    return lower;
}

// One bit per letter and digit, one for '_', one for anything else
static uint64_t CharMask(const std::u32string_view &lower) {
    uint64_t mask = 0;
    for (auto ch : lower) {
        if ((ch >= U'a') && (ch <= U'z')) {
            mask |= 1ull << (ch - U'a');
        } else if ((ch >= U'0') && (ch <= U'9')) {
            mask |= 1ull << (26 + (ch - U'0'));
        } else if (ch == U'_') {
            mask |= 1ull << 36;
        } else {
            mask |= 1ull << 37;
        }
    }
    return mask;
}

static bool IsUpper(char32_t ch) {
    return (ch >= U'A') && (ch <= U'Z');
}

static bool IsLowerOrDigit(char32_t ch) {
    return ((ch >= U'a') && (ch <= U'z')) || ((ch >= U'0') && (ch <= U'9'));
}

// 'SearchFor' -> S, F. 'HTTPServer' -> H, S. 'word_start' -> w, s.
static bool IsWordStart(const std::u32string_view &name, size_t idx) {
    if (idx == 0) {
        return true;
    }
    auto ch = name[idx];
    auto previous = name[idx - 1];
    if ((previous == U'_') || (previous == U'~')) {
        return ch != U'_';
    }
    if (!IsUpper(ch)) {
        return false;
    }
    bool isNextLower = ((idx + 1) < name.size()) && IsLowerOrDigit(name[idx + 1]);
    return IsLowerOrDigit(previous) || (IsUpper(previous) && isNextLower);
}

// Each query char continues the run it is in, or starts at the next word
static bool MatchesWordStarts(const std::u32string_view &lowerQuery, const std::u32string_view &name, const std::u32string_view &lowerName) {
    size_t idxName = 0;
    bool isInRun = false;
    for (auto ch : lowerQuery) {
        if (isInRun && (idxName < lowerName.size()) && (lowerName[idxName] == ch)) {
            idxName++;
            continue;
        }
        while ((idxName < lowerName.size()) && ((lowerName[idxName] != ch) || !IsWordStart(name, idxName))) {
            idxName++;
        }
        if (idxName == lowerName.size()) {
            return false;
        }
        idxName++;
        isInRun = true;
    }
    return true;
}

static bool MatchesSubsequence(const std::u32string_view &lowerQuery, const std::u32string_view &lowerName) {
    size_t idxQuery = 0;
    for (auto ch : lowerName) {
        if (ch == lowerQuery[idxQuery]) {
            idxQuery++;
            if (idxQuery == lowerQuery.size()) {
                return true;
            }
        }
    }
    return false;
}

CodeIndex::kMatchQuality CodeIndex::MatchName(const std::u32string_view &lowerQuery, const std::u32string_view &name, const std::u32string_view &lowerName) {
    if (lowerQuery.empty() || (lowerQuery.size() > lowerName.size())) {
        return kMatchQuality::kNone;
    }
    if (lowerName.compare(0, lowerQuery.size(), lowerQuery) == 0) {
        return (lowerQuery.size() == lowerName.size()) ? kMatchQuality::kExact : kMatchQuality::kPrefix;
    }
    if (MatchesWordStarts(lowerQuery, name, lowerName)) {
        return kMatchQuality::kWordStarts;
    }
    if (lowerName.find(lowerQuery) != std::u32string_view::npos) {
        return kMatchQuality::kSubstring;
    }
    if (MatchesSubsequence(lowerQuery, lowerName)) {
        return kMatchQuality::kSubsequence;
    }
    return kMatchQuality::kNone;
}

// Types and functions first, then what lives inside them, macros last
static uint8_t KindRank(kSymbolKind kind) {
    switch (kind) {
        case kSymbolKind::kClass :
        case kSymbolKind::kStruct :
        case kSymbolKind::kUnion :
        case kSymbolKind::kEnum :
        case kSymbolKind::kAlias :
        case kSymbolKind::kFunction :
        case kSymbolKind::kMethod :
            return 0;
        case kSymbolKind::kNamespace :
        case kSymbolKind::kEnumerator :
        case kSymbolKind::kMember :
        case kSymbolKind::kVariable :
            return 1;
        case kSymbolKind::kMacro :
            return 2;
    }
    return 2;
}

// 'gedit::Document' -> 'Document'
static std::u32string_view InnermostScope(const std::u32string_view &scope) {
    auto idxSeparator = scope.rfind(U"::");
    return (idxSeparator == std::u32string_view::npos) ? scope : scope.substr(idxSeparator + 2);
}

// 'gedit::Document::SearchFor' -> 'gedit::Document'
static std::u32string_view ScopeOf(const CodeSymbol &symbol) {
    std::u32string_view qualified = symbol.qualifiedName;
    if ((qualified.size() < symbol.name.size()) || (qualified.substr(qualified.size() - symbol.name.size()) != symbol.name)) {
        return {};
    }
    qualified.remove_suffix(symbol.name.size());
    if ((qualified.size() >= 2) && (qualified.substr(qualified.size() - 2) == U"::")) {
        qualified.remove_suffix(2);
    }
    return qualified;
}

void CodeIndex::ReplaceFile(const std::string &path, const FileStructure &structure) {
    std::unique_lock guard(lock);

    auto itFile = fileIds.find(path);
    uint32_t idFile = 0;
    if (itFile == fileIds.end()) {
        idFile = (uint32_t)files.size();
        files.emplace_back();
        files.back().path = path;
        fileIds[path] = idFile;
    } else {
        idFile = itFile->second;
        RemoveSymbols(idFile);
    }

    auto &file = files[idFile];
    file.symbols.reserve(structure.symbols.size());
    for (auto &codeSymbol : structure.symbols) {
        // Anonymous types have nothing to search for
        if (codeSymbol.name.empty()) {
            continue;
        }
        Symbol symbol;
        symbol.idName = InternName(codeSymbol.name);
        symbol.idScope = InternScope(std::u32string(ScopeOf(codeSymbol)));
        symbol.line = codeSymbol.line;
        symbol.column = codeSymbol.column;
        symbol.kind = codeSymbol.kind;
        symbol.isDefinition = codeSymbol.isDefinition;
        names[symbol.idName].symbols.push_back({idFile, (uint32_t)file.symbols.size()});
        file.symbols.push_back(symbol);
    }
    numSymbols += file.symbols.size();
    generation++;
}

void CodeIndex::RemoveFile(const std::string &path) {
    std::unique_lock guard(lock);
    auto itFile = fileIds.find(path);
    if (itFile != fileIds.end()) {
        RemoveSymbols(itFile->second);
        generation++;
    }
}

void CodeIndex::RemoveSymbols(uint32_t idFile) {
    auto &file = files[idFile];
    numSymbols -= file.symbols.size();
    for (auto &symbol : file.symbols) {
        auto &name = names[symbol.idName];
        // Several symbols of this file can share the name - the first one takes them all out
        std::erase_if(name.symbols, [idFile](const SymbolHandle &handle) {
            return handle.idFile == idFile;
        });
        if (name.symbols.empty() && !name.text.empty()) {
            nameIds.erase(name.text);
            name.text.clear();
            name.lower.clear();
            name.charMask = 0;
            freeNames.push_back(symbol.idName);
        }
    }
    file.symbols.clear();
}

uint32_t CodeIndex::InternName(const std::u32string &text) {
    auto it = nameIds.find(text);
    if (it != nameIds.end()) {
        return it->second;
    }
    uint32_t idName = 0;
    if (freeNames.empty()) {
        idName = (uint32_t)names.size();
        names.emplace_back();
    } else {
        idName = freeNames.back();
        freeNames.pop_back();
    }
    auto &name = names[idName];
    name.text = text;
    name.lower = Lower(text);
    name.charMask = CharMask(name.lower);
    nameIds[name.text] = idName;
    return idName;
}

uint32_t CodeIndex::InternScope(const std::u32string &text) {
    auto it = scopeIds.find(text);
    if (it != scopeIds.end()) {
        return it->second;
    }
    auto idScope = (uint32_t)scopes.size();
    scopes.push_back({text, Lower(text)});
    scopeIds[scopes.back().text] = idScope;
    return idScope;
}

std::vector<CodeIndex::Hit> CodeIndex::Query(const std::u32string &query, const QueryContext &context) const {
    auto lowerQuery = Lower(query);
    std::u32string_view lowerName = lowerQuery;
    std::u32string_view lowerScope;
    auto idxSeparator = lowerName.rfind(U"::");
    bool isScoped = idxSeparator != std::u32string_view::npos;
    if (isScoped) {
        lowerScope = lowerName.substr(0, idxSeparator);
        lowerName = lowerName.substr(idxSeparator + 2);
    }
    if (lowerName.empty() && lowerScope.empty()) {
        return {};
    }
    auto queryMask = CharMask(lowerName);
    auto activeFolder = std::filesystem::path(context.activePath).parent_path().string();

    std::shared_lock guard(lock);

    std::vector<bool> scopeMatches;
    if (!lowerScope.empty()) {
        scopeMatches = MatchingScopes(std::u32string(lowerScope));
    }

    static constexpr uint8_t PROXIMITY_UNKNOWN = 0xff;
    std::vector<uint8_t> proximity(files.size(), PROXIMITY_UNKNOWN);

    std::vector<Candidate> candidates;
    for (auto &name : names) {
        if (name.symbols.empty() || ((name.charMask & queryMask) != queryMask)) {
            continue;
        }
        // 'Scope::' alone lists the scope - any name will do
        auto quality = lowerName.empty() ? kMatchQuality::kPrefix : MatchName(lowerName, name.text, name.lower);
        if (quality == kMatchQuality::kNone) {
            continue;
        }
        for (auto &handle : name.symbols) {
            auto &file = files[handle.idFile];
            auto &symbol = file.symbols[handle.idxSymbol];
            if (!scopeMatches.empty() && !scopeMatches[symbol.idScope]) {
                continue;
            }
            if (proximity[handle.idFile] == PROXIMITY_UNKNOWN) {
                proximity[handle.idFile] = Proximity(file, activeFolder, context.activePath);
            }
            candidates.push_back({quality, KindRank(symbol.kind), proximity[handle.idFile], symbol.isDefinition, (uint32_t)name.text.size(), handle});
        }
    }

    auto numHits = std::min(candidates.size(), context.maxHits);
    std::partial_sort(candidates.begin(), candidates.begin() + (ptrdiff_t)numHits, candidates.end(), [this](const Candidate &a, const Candidate &b) {
        return IsBetter(a, b);
    });

    std::vector<Hit> hits;
    hits.reserve(numHits);
    for (size_t i = 0; i < numHits; i++) {
        hits.push_back(MakeHit(candidates[i].handle));
    }
    return hits;
}

bool CodeIndex::IsBetter(const Candidate &a, const Candidate &b) const {
    if (a.quality != b.quality) {
        return a.quality > b.quality;
    }
    if (a.kindRank != b.kindRank) {
        return a.kindRank < b.kindRank;
    }
    if (a.proximity != b.proximity) {
        return a.proximity < b.proximity;
    }
    if (a.isDefinition != b.isDefinition) {
        return a.isDefinition;
    }
    if (a.nameLength != b.nameLength) {
        return a.nameLength < b.nameLength;
    }
    // Same order for the same index: the list must not shuffle between two keypresses
    auto &fileA = files[a.handle.idFile];
    auto &fileB = files[b.handle.idFile];
    auto &symbolA = fileA.symbols[a.handle.idxSymbol];
    auto &symbolB = fileB.symbols[b.handle.idxSymbol];
    if (symbolA.idName != symbolB.idName) {
        return names[symbolA.idName].text < names[symbolB.idName].text;
    }
    if (a.handle.idFile != b.handle.idFile) {
        return fileA.path < fileB.path;
    }
    return a.handle.idxSymbol < b.handle.idxSymbol;
}

std::vector<bool> CodeIndex::MatchingScopes(const std::u32string &lowerScopeQuery) const {
    std::vector<bool> matches(scopes.size(), false);
    for (size_t i = 0; i < scopes.size(); i++) {
        matches[i] = scopes[i].lower.find(lowerScopeQuery) != std::u32string::npos;
    }
    return matches;
}

// 0: the active file, 1: its folder, 2: elsewhere
uint8_t CodeIndex::Proximity(const File &file, const std::string &activeFolder, const std::string &activePath) const {
    if (activePath.empty()) {
        return 2;
    }
    if (file.path == activePath) {
        return 0;
    }
    bool isSameFolder = (file.path.size() > activeFolder.size()) && (file.path.compare(0, activeFolder.size(), activeFolder) == 0) &&
                        (file.path[activeFolder.size()] == '/') && (file.path.find('/', activeFolder.size() + 1) == std::string::npos);
    return isSameFolder ? 1 : 2;
}

CodeIndex::Hit CodeIndex::MakeHit(const SymbolHandle &handle) const {
    auto &file = files[handle.idFile];
    auto &symbol = file.symbols[handle.idxSymbol];
    auto &name = names[symbol.idName];
    auto &scope = scopes[symbol.idScope];

    Hit hit;
    hit.name = name.text;
    hit.qualifiedName = scope.text.empty() ? name.text : (scope.text + U"::" + name.text);
    hit.path = file.path;
    hit.kind = symbol.kind;
    hit.line = symbol.line;
    hit.column = symbol.column;
    hit.isDefinition = symbol.isDefinition;
    // One file can't tell 'A::f' of a class from one of a namespace (FileStructure.h) - the index can
    if ((hit.kind == kSymbolKind::kFunction) && IsClassScope(scope.text)) {
        hit.kind = kSymbolKind::kMethod;
    }
    return hit;
}

// The innermost part of the scope names a class, struct or union somewhere in the index
bool CodeIndex::IsClassScope(const std::u32string &scope) const {
    if (scope.empty()) {
        return false;
    }
    auto itName = nameIds.find(InnermostScope(scope));
    if (itName == nameIds.end()) {
        return false;
    }
    for (auto &handle : names[itName->second].symbols) {
        auto kind = files[handle.idFile].symbols[handle.idxSymbol].kind;
        if ((kind == kSymbolKind::kClass) || (kind == kSymbolKind::kStruct) || (kind == kSymbolKind::kUnion)) {
            return true;
        }
    }
    return false;
}

WorkspaceNames::Ref CodeIndex::GetWorkspaceNames() {
    std::lock_guard namesGuard(workspaceNamesLock);
    std::shared_lock guard(lock);
    if ((workspaceNames != nullptr) && (generationOfNames == generation)) {
        return workspaceNames;
    }
    auto current = std::make_shared<WorkspaceNames>();
    for (auto &name : names) {
        auto meaning = SingleMeaning(name);
        if (meaning.has_value()) {
            current->Add(name.text, *meaning);
        }
    }
    generationOfNames = generation;
    // Most changes - any pause in typing is one - leave every meaning as it was
    if ((workspaceNames == nullptr) || !(*current == *workspaceNames)) {
        workspaceNames = current;
    }
    return workspaceNames;
}

// A constructor is its class, not a second meaning. A namespace-scope variable has no colour of its own but it
// is a meaning: a name that is also a variable can't be told from it.
std::optional<kLanguageTokenClass> CodeIndex::SingleMeaning(const Name &name) const {
    std::optional<kLanguageTokenClass> meaning;
    for (auto &handle : name.symbols) {
        auto &symbol = files[handle.idFile].symbols[handle.idxSymbol];
        bool isCallable = (symbol.kind == kSymbolKind::kFunction) || (symbol.kind == kSymbolKind::kMethod);
        if (isCallable && (InnermostScope(scopes[symbol.idScope].text) == name.text)) {
            continue;
        }
        auto tokenClass = SemanticNames::TokenClassFor(symbol.kind);
        if (meaning.has_value() && (*meaning != tokenClass)) {
            return {};
        }
        meaning = tokenClass;
    }
    if (meaning == kLanguageTokenClass::kRegular) {
        return {};
    }
    return meaning;
}

CodeIndex::Stats CodeIndex::GetStats() const {
    std::shared_lock guard(lock);
    Stats stats;
    stats.numSymbols = numSymbols;
    stats.numNames = nameIds.size();
    stats.numScopes = scopes.size();
    stats.numBytes = numSymbols * (sizeof(Symbol) + sizeof(SymbolHandle));
    for (auto &file : files) {
        if (!file.symbols.empty()) {
            stats.numFiles++;
        }
        stats.numBytes += sizeof(File) + file.path.capacity();
    }
    for (auto &name : names) {
        stats.numBytes += sizeof(Name) + (name.text.capacity() + name.lower.capacity()) * sizeof(char32_t);
    }
    for (auto &scope : scopes) {
        stats.numBytes += sizeof(Scope) + (scope.text.capacity() + scope.lower.capacity()) * sizeof(char32_t);
    }
    return stats;
}

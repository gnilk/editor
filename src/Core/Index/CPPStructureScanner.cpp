//
// C/C++ structure pass - see CPPStructureScanner.h
//

#include <algorithm>

#include "CPPStructureScanner.h"

using namespace gedit;

namespace {
    // Ownership idioms (docs/code-index.md §5.4) - tables in code for now, config later (step 7)
    const std::vector<std::u32string> kOwningPointers = { U"unique_ptr", U"auto_ptr" };
    const std::vector<std::u32string> kSharingPointers = { U"shared_ptr", U"weak_ptr" };
    const std::vector<std::u32string> kContainers = {
        U"vector", U"list", U"deque", U"array", U"set", U"unordered_set", U"multiset", U"map", U"unordered_map",
        U"multimap", U"optional", U"stack", U"queue", U"priority_queue", U"forward_list", U"span",
    };
    // Project idioms: 'T::Ref' is a shared_ptr<T>, 'T::Ptr' a unique_ptr<T> (GoatEdit)
    const std::u32string kRefAlias = U"Ref";
    const std::u32string kPtrAlias = U"Ptr";

    // Leading words that make a statement not a type ('static int x' still has 'int')
    const std::vector<std::u32string> kSpecifiers = {
        U"virtual", U"static", U"inline", U"explicit", U"constexpr", U"consteval", U"constinit", U"friend",
        U"extern", U"mutable", U"thread_local", U"register",
    };
    const std::vector<std::u32string> kTypeQualifiers = {
        U"const", U"volatile", U"static", U"mutable", U"inline", U"constexpr", U"typename", U"struct", U"class",
        U"enum", U"union", U"extern", U"thread_local", U"register", U"unsigned", U"signed",
    };

    bool Contains(const std::vector<std::u32string> &list, const std::u32string &text) {
        return std::find(list.begin(), list.end(), text) != list.end();
    }

    bool IsAllCaps(const std::u32string &text) {
        bool hasLetter = false;
        for (auto ch : text) {
            if ((ch >= U'a') && (ch <= U'z')) {
                return false;
            }
            if ((ch >= U'A') && (ch <= U'Z')) {
                hasLetter = true;
            }
        }
        return hasLetter && (text.size() > 1);
    }

    bool EndsWithBackslash(const CPPStructureScanner::Token &token) {
        return !token.text.empty() && (token.text.back() == U'\\');
    }

    std::u32string LastComponent(const std::u32string &qualifiedName) {
        auto idx = qualifiedName.rfind(U"::");
        return (idx == std::u32string::npos) ? qualifiedName : qualifiedName.substr(idx + 2);
    }

    std::u32string Qualifier(const std::u32string &qualifiedName) {
        auto idx = qualifiedName.rfind(U"::");
        return (idx == std::u32string::npos) ? std::u32string() : qualifiedName.substr(0, idx);
    }

    // A type as written: 'std::vector<Foo::Ref> &'
    struct TypeRef {
        std::u32string name;
        std::vector<TypeRef> args;
        int numPointers = 0;        // '*' and '&'
        bool isBuiltin = false;     // int, void, auto, ...
    };

    struct TypeToken {
        std::u32string text;
        bool isBuiltin = false;
    };

    // Recursive descent over 'tokens' from 'idx': name [ '<' args '>' ] { '*' | '&' | '&&' | const }
    TypeRef ParseType(const std::vector<TypeToken> &tokens, size_t &idx) {
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
            if (text.empty() || !(std::isalnum((int)(text[0] & 0x7f)) || (text[0] == U'_') || (text[0] > 127))) {
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
                type.args.push_back(ParseType(tokens, idx));
                if (idx == before) {
                    // Something we don't understand (a value argument) - skip it
                    idx++;
                }
            }
            // 'Foo<T>::Ref'
            if ((idx + 1 < tokens.size()) && (tokens[idx].text == U"::")) {
                idx++;
                auto nested = ParseType(tokens, idx);
                type.name += U"::" + nested.name;
                type.args.clear();
                type.numPointers = nested.numPointers;
                return type;
            }
        }
        while (idx < tokens.size()) {
            auto &text = tokens[idx].text;
            if ((text == U"*") || (text == U"&") || (text == U"&&")) {
                type.numPointers++;
            } else if ((text != U"const") && (text != U"volatile")) {
                break;
            }
            idx++;
        }
        return type;
    }

    // What a member of this type means for the class holding it: the held type and how
    std::pair<std::u32string, kOwnership> ResolveHeld(const TypeRef &type, bool isPointer) {
        isPointer = isPointer || (type.numPointers > 0);
        auto last = LastComponent(type.name);
        auto qualifier = Qualifier(type.name);
        if (type.isBuiltin || type.name.empty()) {
            return {};
        }
        if (Contains(kOwningPointers, last) && !type.args.empty()) {
            auto [target, ownership] = ResolveHeld(type.args[0], false);
            return {target, isPointer ? kOwnership::kBorrows : kOwnership::kOwns};
        }
        if (Contains(kSharingPointers, last) && !type.args.empty()) {
            auto [target, ownership] = ResolveHeld(type.args[0], false);
            return {target, isPointer ? kOwnership::kBorrows : kOwnership::kShares};
        }
        if ((last == kRefAlias) && !qualifier.empty()) {
            return {qualifier, isPointer ? kOwnership::kBorrows : kOwnership::kShares};
        }
        if ((last == kPtrAlias) && !qualifier.empty()) {
            return {qualifier, isPointer ? kOwnership::kBorrows : kOwnership::kOwns};
        }
        if (Contains(kContainers, last) && !type.args.empty()) {
            // The element (for a map: the value)
            auto [target, ownership] = ResolveHeld(type.args.back(), false);
            return {target, isPointer ? kOwnership::kBorrows : ownership};
        }
        // Other library types (std::string, std::mutex, std::function) aren't structure of this code base
        if (type.name.rfind(U"std::", 0) == 0) {
            return {};
        }
        return {type.name, isPointer ? kOwnership::kBorrows : kOwnership::kOwns};
    }
}

const char *gedit::SymbolKindName(kSymbolKind kind) {
    switch (kind) {
        case kSymbolKind::kNamespace : return "namespace";
        case kSymbolKind::kClass : return "class";
        case kSymbolKind::kStruct : return "struct";
        case kSymbolKind::kUnion : return "union";
        case kSymbolKind::kEnum : return "enum";
        case kSymbolKind::kEnumerator : return "enumerator";
        case kSymbolKind::kFunction : return "function";
        case kSymbolKind::kMethod : return "method";
        case kSymbolKind::kMember : return "member";
        case kSymbolKind::kVariable : return "variable";
        case kSymbolKind::kAlias : return "alias";
        case kSymbolKind::kMacro : return "macro";
    }
    return "?";
}

const char *gedit::EdgeKindName(kEdgeKind kind) {
    switch (kind) {
        case kEdgeKind::kInherits : return "inherits";
        case kEdgeKind::kHolds : return "holds";
        case kEdgeKind::kIncludes : return "includes";
    }
    return "?";
}

const char *gedit::OwnershipName(kOwnership ownership) {
    switch (ownership) {
        case kOwnership::kNone : return "";
        case kOwnership::kOwns : return "owns";
        case kOwnership::kShares : return "shares";
        case kOwnership::kBorrows : return "borrows";
    }
    return "?";
}

CPPStructureScanner::CPPStructureScanner(const LangLineTokenizer &tokenizer) : tokenizer(tokenizer) {
}

FileStructure CPPStructureScanner::Scan(const std::u32string &text) {
    result = {};
    tokens.clear();
    scopes.clear();
    statement.clear();
    enumItem.clear();
    parenDepth = 0;
    pendingTypedefSymbol = -1;
    isAfterAnonymousType = false;
    conditionals.clear();
    isContinuation = false;

    CollectTokens(text);
    ScanCode();
    return std::move(result);
}

//
// Pass 1: the lexer's tokens, a line at a time - preprocessor lines are read here, comments and strings
// dropped, the rest kept for the code pass
//
void CPPStructureScanner::CollectTokens(const std::u32string &text) {
    std::vector<Token> lineTokens;
    size_t idxCurrentLine = 0;
    result.numLines = tokenizer.TokenizeText(text, [this, &lineTokens, &idxCurrentLine](size_t idxLine, const LangToken &token) {
        if ((idxLine != idxCurrentLine) && !lineTokens.empty()) {
            FlushLine(lineTokens);
        }
        idxCurrentLine = idxLine;
        lineTokens.push_back({token.string, (uint32_t)idxLine, (uint32_t)token.idxOrigStr, token.classification});
    });
    FlushLine(lineTokens);
}

void CPPStructureScanner::FlushLine(std::vector<Token> &lineTokens) {
    if (lineTokens.empty()) {
        return;
    }
    // The rest of a '\' continued preprocessor line (a multi-line #define)
    if (isContinuation) {
        isContinuation = EndsWithBackslash(lineTokens.back());
        lineTokens.clear();
        return;
    }
    if ((lineTokens[0].text == U"#") && (lineTokens[0].classification == kLanguageTokenClass::kPreProcessor)) {
        ScanPreprocessorLine(lineTokens);
        isContinuation = EndsWithBackslash(lineTokens.back());
        lineTokens.clear();
        return;
    }
    if (!IsSkipping()) {
        for (auto &token : lineTokens) {
            switch (token.classification) {
                case kLanguageTokenClass::kLineComment :
                case kLanguageTokenClass::kBlockComment :
                case kLanguageTokenClass::kCommentedText :
                case kLanguageTokenClass::kString :
                case kLanguageTokenClass::kChar :
                    continue;
                default:
                    break;
            }
            // The quotes around strings and chars
            if ((token.text == U"\"") || (token.text == U"'")) {
                continue;
            }
            tokens.push_back(std::move(token));
        }
    }
    lineTokens.clear();
}

// Conditionals keep the first arm ('#if 0' keeps the '#else') - braces can't be balanced across arms
void CPPStructureScanner::ScanPreprocessorLine(const std::vector<Token> &lineTokens) {
    if (lineTokens.size() < 2) {
        return;
    }
    auto &directive = lineTokens[1].text;
    if ((directive == U"if") || (directive == U"ifdef") || (directive == U"ifndef")) {
        bool isZero = (directive == U"if") && (lineTokens.size() > 2) && (lineTokens[2].text == U"0");
        conditionals.push_back({isZero, !isZero});
        return;
    }
    if ((directive == U"elif") || (directive == U"else")) {
        if (conditionals.empty()) {
            return;
        }
        auto &conditional = conditionals.back();
        if (conditional.isTaken) {
            conditional.isSkipping = true;
        } else {
            conditional.isSkipping = false;
            conditional.isTaken = true;
        }
        return;
    }
    if (directive == U"endif") {
        if (!conditionals.empty()) {
            conditionals.pop_back();
        }
        return;
    }
    if (IsSkipping()) {
        return;
    }
    if ((directive == U"define") && (lineTokens.size() > 2)) {
        auto &nameToken = lineTokens[2];
        auto name = nameToken.text.substr(0, nameToken.text.find(U'('));
        if (!name.empty()) {
            CodeSymbol symbol;
            symbol.name = name;
            symbol.qualifiedName = name;
            symbol.kind = kSymbolKind::kMacro;
            symbol.line = nameToken.line;
            symbol.column = nameToken.column;
            symbol.beginLine = nameToken.line;
            symbol.endLine = nameToken.line;
            symbol.isDefinition = true;
            result.symbols.push_back(std::move(symbol));
        }
        return;
    }
    if (directive == U"include") {
        CodeEdge edge;
        edge.kind = kEdgeKind::kIncludes;
        edge.line = lineTokens[0].line;
        for (size_t i = 2; i < lineTokens.size(); i++) {
            auto &text = lineTokens[i].text;
            if (text == U"<") {
                edge.isSystemInclude = true;
            } else if ((text != U">") && (text != U"\"")) {
                edge.toName += text;
            }
        }
        if (!edge.toName.empty()) {
            result.edges.push_back(std::move(edge));
        }
    }
}

bool CPPStructureScanner::IsSkipping() const {
    return std::any_of(conditionals.begin(), conditionals.end(), [](const Conditional &conditional) {
        return conditional.isSkipping;
    });
}

//
// Pass 2: the code tokens - a scope stack and the statement being collected in the current scope
//
void CPPStructureScanner::ScanCode() {
    scopes.push_back({});
    for (size_t i = 0; i < tokens.size(); i++) {
        auto &token = tokens[i];
        auto scopeKind = CurrentScope().kind;
        bool isOpen = (token.classification == kLanguageTokenClass::kCodeBlockStart);
        bool isClose = (token.classification == kLanguageTokenClass::kCodeBlockEnd);

        // Inside a body we only count braces
        if ((scopeKind == kScope::kOpaque) || (scopeKind == kScope::kInitializer)) {
            if (isOpen) {
                scopes.push_back({kScope::kOpaque});
            } else if (isClose) {
                OnCloseBrace(i);
            }
            continue;
        }
        if (isOpen) {
            OnOpenBrace(i);
            continue;
        }
        if (isClose) {
            OnCloseBrace(i);
            continue;
        }
        if (token.text == U"(") {
            parenDepth++;
        } else if (token.text == U")") {
            parenDepth = std::max(0, parenDepth - 1);
        }
        if (scopeKind == kScope::kEnum) {
            if ((token.text == U",") && (parenDepth == 0)) {
                OnEnumItemEnd();
            } else {
                enumItem.push_back(i);
            }
            continue;
        }
        if ((token.text == U";") && (parenDepth == 0)) {
            OnSemicolon();
            continue;
        }
        // 'public:' - the access specifier isn't part of the next declaration
        if ((token.text == U":") && InClassScope() && !statement.empty()) {
            auto &last = tokens[statement.back()].text;
            if ((last == U"public") || (last == U"protected") || (last == U"private")) {
                statement.clear();
                continue;
            }
        }
        statement.push_back(i);
    }
}

void CPPStructureScanner::OnOpenBrace(size_t idxToken) {
    // Braces in parentheses: a default argument, a lambda - part of the statement
    if (parenDepth > 0) {
        scopes.push_back({kScope::kInitializer});
        return;
    }
    auto stmt = StripPrefixes(statement);

    if ((stmt.size() == 1) && IsText(stmt, 0, U"extern")) {
        scopes.push_back({kScope::kExtern});
        statement.clear();
        return;
    }

    size_t idxNamespace = IsText(stmt, 0, U"inline") ? 1 : 0;
    if (IsText(stmt, idxNamespace, U"namespace")) {
        std::u32string name;
        int idxSymbol = -1;
        for (size_t i = idxNamespace + 1; i < stmt.size(); i++) {
            if (IsIdentifier(stmt, i) || IsText(stmt, i, U"::")) {
                name += At(stmt, i).text;
            }
        }
        if (!name.empty()) {
            idxSymbol = AddSymbol(kSymbolKind::kNamespace, LastComponent(name), Qualifier(name), At(stmt, stmt.size() - 1), true);
        }
        scopes.push_back({kScope::kNamespace, idxSymbol, name});
        statement.clear();
        return;
    }

    // Find the top level (outside parentheses and template arguments) '=', '(' and a ') :' constructor
    // initializer list
    bool hasAssign = false;
    bool hasParen = false;
    bool hasInitList = false;
    int depth = 0;
    for (size_t i = 0; i < stmt.size(); i++) {
        auto &text = At(stmt, i).text;
        if (text == U"(") {
            hasParen = hasParen || (depth == 0);
            depth++;
        } else if (text == U")") {
            depth--;
            if ((depth == 0) && IsText(stmt, i + 1, U":")) {
                hasInitList = true;
            }
        } else if ((text == U"=") && (depth == 0)) {
            hasAssign = true;
        }
    }
    if (hasAssign) {
        scopes.push_back({kScope::kInitializer});
        return;
    }
    if (TryClassDefinition(stmt, idxToken) || TryEnumDefinition(stmt)) {
        statement.clear();
        parenDepth = 0;
        return;
    }
    // 'Foo f{1}', 'int a{0}', or 'b{2}' in a constructor's initializer list
    if (!stmt.empty() && (!hasParen || hasInitList)) {
        auto &last = At(stmt, stmt.size() - 1);
        if (IsIdentifier(stmt, stmt.size() - 1) || (last.text == U">")) {
            scopes.push_back({kScope::kInitializer});
            return;
        }
    }

    int idxSymbol = -1;
    TryFunction(stmt, true, idxSymbol);
    scopes.push_back({kScope::kOpaque, idxSymbol});
    statement.clear();
    parenDepth = 0;
}

void CPPStructureScanner::OnCloseBrace(size_t idxToken) {
    if (scopes.size() <= 1) {
        return;
    }
    if (CurrentScope().kind == kScope::kEnum) {
        OnEnumItemEnd();
    }
    auto scope = scopes.back();
    scopes.pop_back();
    if (scope.idxSymbol >= 0) {
        result.symbols[scope.idxSymbol].endLine = tokens[idxToken].line;
    }
    if (scope.kind == kScope::kInitializer) {
        // The statement goes on - leave a marker so 'b{2} {' sees the '}' before the body
        if ((CurrentScope().kind != kScope::kOpaque) && (CurrentScope().kind != kScope::kInitializer)) {
            statement.push_back(idxToken);
        }
        return;
    }
    if (scope.kind == kScope::kOpaque) {
        return;
    }
    if (scope.isTypedef && (scope.idxSymbol >= 0)) {
        pendingTypedefSymbol = scope.idxSymbol;
    } else if ((scope.kind == kScope::kClass) && (scope.idxSymbol >= 0) && result.symbols[scope.idxSymbol].name.empty()) {
        isAfterAnonymousType = true;
    }
    statement.clear();
    parenDepth = 0;
}

void CPPStructureScanner::OnSemicolon() {
    auto stmt = StripPrefixes(statement);
    statement.clear();
    parenDepth = 0;
    if (pendingTypedefSymbol >= 0) {
        OnTypedefName(stmt);
        pendingTypedefSymbol = -1;
        return;
    }
    if (isAfterAnonymousType) {
        isAfterAnonymousType = false;
        auto kind = InClassScope() ? kSymbolKind::kMember : kSymbolKind::kVariable;
        for (size_t i = 0; i < stmt.size(); i++) {
            if (IsIdentifier(stmt, i) && !IsText(stmt, i + 1, U"::")) {
                AddSymbol(kind, At(stmt, i).text, U"", At(stmt, i), true);
            }
        }
        return;
    }
    if (!stmt.empty()) {
        ClassifyDeclaration(stmt);
    }
}

// 'typedef struct { ... } Name;' names the struct; 'typedef struct Foo { ... } Foo_t;' adds an alias
void CPPStructureScanner::OnTypedefName(const Statement &stmt) {
    for (size_t i = stmt.size(); i > 0; i--) {
        if (!IsIdentifier(stmt, i - 1)) {
            continue;
        }
        auto &symbol = result.symbols[pendingTypedefSymbol];
        auto &nameToken = At(stmt, i - 1);
        if (symbol.name.empty()) {
            // Nameless, it added nothing to its members' qualified names: 'flags' becomes 'Name::flags'. Not
            // for a plain enum - its enumerators belong to the enclosing scope.
            auto prefix = symbol.qualifiedName;
            auto newQualifiedName = prefix.empty() ? nameToken.text : prefix + U"::" + nameToken.text;
            bool isScope = (symbol.kind != kSymbolKind::kEnum);
            for (size_t idx = (size_t)pendingTypedefSymbol + 1; isScope && (idx < result.symbols.size()); idx++) {
                auto &child = result.symbols[idx];
                bool isInside = false;
                for (auto idxAncestor = child.idxParent; idxAncestor >= 0; idxAncestor = result.symbols[idxAncestor].idxParent) {
                    isInside = isInside || (idxAncestor == pendingTypedefSymbol);
                }
                if (!isInside) {
                    continue;
                }
                // A nested anonymous type carries exactly the prefix
                if (child.qualifiedName == prefix) {
                    child.qualifiedName = newQualifiedName;
                } else if (prefix.empty()) {
                    child.qualifiedName = newQualifiedName + U"::" + child.qualifiedName;
                } else if (child.qualifiedName.rfind(prefix + U"::", 0) == 0) {
                    child.qualifiedName = newQualifiedName + child.qualifiedName.substr(prefix.size());
                }
            }
            symbol.name = nameToken.text;
            symbol.qualifiedName = newQualifiedName;
            symbol.line = nameToken.line;
            symbol.column = nameToken.column;
        } else {
            AddSymbol(kSymbolKind::kAlias, nameToken.text, U"", nameToken, true);
        }
        return;
    }
}

void CPPStructureScanner::OnEnumItemEnd() {
    auto item = enumItem;
    enumItem.clear();
    parenDepth = 0;
    if (!item.empty() && IsIdentifier(item, 0)) {
        AddSymbol(kSymbolKind::kEnumerator, At(item, 0).text, U"", At(item, 0), true);
    }
}

// A ';' at file / namespace / class scope
void CPPStructureScanner::ClassifyDeclaration(Statement stmt) {
    if (IsText(stmt, 0, U"using")) {
        if (IsIdentifier(stmt, 1) && IsText(stmt, 2, U"=")) {
            AddSymbol(kSymbolKind::kAlias, At(stmt, 1).text, U"", At(stmt, 1), true);
        }
        return;
    }
    if (IsText(stmt, 0, U"typedef")) {
        // Function pointer: typedef void (*Name)(int);
        for (size_t i = 1; i + 2 < stmt.size(); i++) {
            if (IsText(stmt, i, U"(") && IsText(stmt, i + 1, U"*") && IsIdentifier(stmt, i + 2)) {
                AddSymbol(kSymbolKind::kAlias, At(stmt, i + 2).text, U"", At(stmt, i + 2), true);
                return;
            }
        }
        for (size_t i = stmt.size(); i > 1; i--) {
            if (IsIdentifier(stmt, i - 1)) {
                AddSymbol(kSymbolKind::kAlias, At(stmt, i - 1).text, U"", At(stmt, i - 1), true);
                return;
            }
        }
        return;
    }
    if (IsText(stmt, 0, U"friend") || IsText(stmt, 0, U"static_assert") || IsText(stmt, 0, U"template") ||
        IsText(stmt, 0, U"namespace")) {
        return;
    }
    // Forward declaration: 'class Foo;', 'class EXPORT Foo;'
    bool isClassKey = IsText(stmt, 0, U"class") || IsText(stmt, 0, U"struct") || IsText(stmt, 0, U"union");
    if (isClassKey && (stmt.size() >= 2)) {
        bool isNamesOnly = true;
        for (size_t i = 1; i < stmt.size(); i++) {
            isNamesOnly = isNamesOnly && (IsIdentifier(stmt, i) || IsText(stmt, i, U"::"));
        }
        if (isNamesOnly) {
            auto kind = IsText(stmt, 0, U"class") ? kSymbolKind::kClass : IsText(stmt, 0, U"struct") ? kSymbolKind::kStruct : kSymbolKind::kUnion;
            AddSymbol(kind, At(stmt, stmt.size() - 1).text, U"", At(stmt, stmt.size() - 1), false);
            return;
        }
    }
    int idxSymbol = -1;
    if (TryFunction(stmt, false, idxSymbol)) {
        return;
    }
    AddVariables(stmt);
}

// 'class/struct/union [attributes/macros] Name [final] [: bases] {'
bool CPPStructureScanner::TryClassDefinition(const Statement &stmt, size_t idxBrace) {
    size_t idxKey = 0;
    if (IsText(stmt, 0, U"typedef")) {
        idxKey = 1;
    }
    bool isClass = IsText(stmt, idxKey, U"class");
    bool isStruct = IsText(stmt, idxKey, U"struct");
    bool isUnion = IsText(stmt, idxKey, U"union");
    if (!isClass && !isStruct && !isUnion) {
        return false;
    }
    // The name: the last (qualified) identifier before ':', 'final' or the brace
    std::u32string name;
    const Token *nameToken = nullptr;
    size_t i = idxKey + 1;
    while (i < stmt.size()) {
        if (IsText(stmt, i, U"[") && IsText(stmt, i + 1, U"[")) {
            while ((i < stmt.size()) && !IsText(stmt, i, U"]")) {
                i++;
            }
            i += 2;
            continue;
        }
        if (IsText(stmt, i, U"(")) {
            // alignas(...), DECLSPEC(...)
            int depth = 0;
            do {
                depth += IsText(stmt, i, U"(") ? 1 : IsText(stmt, i, U")") ? -1 : 0;
                i++;
            } while ((i < stmt.size()) && (depth > 0));
            continue;
        }
        if (IsText(stmt, i, U"<")) {
            // A specialisation: 'struct Foo<int>'
            int depth = 0;
            do {
                depth += IsText(stmt, i, U"<") ? 1 : IsText(stmt, i, U">") ? -1 : IsText(stmt, i, U">>") ? -2 : 0;
                i++;
            } while ((i < stmt.size()) && (depth > 0));
            continue;
        }
        if (IsIdentifier(stmt, i)) {
            if (!IsText(stmt, i - 1, U"::")) {
                name.clear();
            }
            name += At(stmt, i).text;
            nameToken = &At(stmt, i);
            i++;
            continue;
        }
        if (IsText(stmt, i, U"::")) {
            name += U"::";
            i++;
            continue;
        }
        break;
    }
    if (IsText(stmt, i, U"final")) {
        i++;
    }
    // Anything else ('struct Foo *make() {') is not a class definition
    if ((i < stmt.size()) && !IsText(stmt, i, U":")) {
        return false;
    }
    auto kind = isClass ? kSymbolKind::kClass : isStruct ? kSymbolKind::kStruct : kSymbolKind::kUnion;
    auto &at = (nameToken != nullptr) ? *nameToken : At(stmt, idxKey);
    auto idxSymbol = AddSymbol(kind, LastComponent(name), Qualifier(name), at, true);
    // The definition starts at the keyword ('typedef struct\n{ ... } Name;' is named at the end)
    result.symbols[idxSymbol].beginLine = At(stmt, idxKey).line;

    // Bases: ': public Base, private Other<T>'
    if (IsText(stmt, i, U":")) {
        i++;
        while (i < stmt.size()) {
            std::u32string baseName;
            const Token *baseToken = nullptr;
            int depth = 0;
            for (; i < stmt.size(); i++) {
                auto &text = At(stmt, i).text;
                if ((depth == 0) && (text == U",")) {
                    i++;
                    break;
                }
                if (text == U"<") {
                    depth++;
                } else if (text == U">") {
                    depth--;
                } else if (text == U">>") {
                    depth -= 2;
                } else if ((depth == 0) && (IsIdentifier(stmt, i) || (text == U"::"))) {
                    baseName += text;
                    if (baseToken == nullptr) {
                        baseToken = &At(stmt, i);
                    }
                }
            }
            if (!baseName.empty()) {
                CodeEdge edge;
                edge.kind = kEdgeKind::kInherits;
                edge.idxFrom = idxSymbol;
                edge.toName = baseName;
                edge.line = baseToken->line;
                result.edges.push_back(std::move(edge));
            }
        }
    }
    scopes.push_back({kScope::kClass, idxSymbol, LastComponent(name), false, idxKey == 1});
    return true;
}

// 'enum [class|struct] [Name] [: type] {'
bool CPPStructureScanner::TryEnumDefinition(const Statement &stmt) {
    size_t i = IsText(stmt, 0, U"typedef") ? 1 : 0;
    if (!IsText(stmt, i, U"enum")) {
        return false;
    }
    i++;
    bool isEnumClass = IsText(stmt, i, U"class") || IsText(stmt, i, U"struct");
    if (isEnumClass) {
        i++;
    }
    std::u32string name;
    const Token *at = &At(stmt, 0);
    if (IsIdentifier(stmt, i)) {
        name = At(stmt, i).text;
        at = &At(stmt, i);
        i++;
    }
    if ((i < stmt.size()) && !IsText(stmt, i, U":")) {
        return false;
    }
    auto idxSymbol = AddSymbol(kSymbolKind::kEnum, name, U"", *at, true);
    scopes.push_back({kScope::kEnum, idxSymbol, name, isEnumClass, IsText(stmt, 0, U"typedef")});
    enumItem.clear();
    return true;
}

// '[type] [Qualifier::]name ( ... ) [trailer]' - true when the statement has that shape (even if it adds no
// symbol: a macro call 'DECLARE(x);' is a function shape without a type)
bool CPPStructureScanner::TryFunction(const Statement &stmt, bool isDefinition, int &outIdxSymbol) {
    outIdxSymbol = -1;

    // The top level '(' of the parameter list - outside template arguments, after 'operator()'
    size_t idxParen = stmt.size();
    size_t idxAssign = stmt.size();
    size_t idxOperator = stmt.size();
    int angleDepth = 0;
    for (size_t i = 0; i < stmt.size(); i++) {
        auto &text = At(stmt, i).text;
        if (text == U"operator") {
            idxOperator = i;
            if (IsText(stmt, i + 1, U"(") && IsText(stmt, i + 2, U")")) {
                i += 2;
            }
            continue;
        }
        if ((text == U"<") && (i > 0) && (IsIdentifier(stmt, i - 1) || (At(stmt, i - 1).classification == kLanguageTokenClass::kKnownType))) {
            angleDepth++;
        } else if ((text == U">") && (angleDepth > 0)) {
            angleDepth--;
        } else if ((text == U">>") && (angleDepth > 0)) {
            angleDepth = std::max(0, angleDepth - 2);
        } else if ((text == U"=") && (angleDepth == 0) && (idxAssign == stmt.size())) {
            idxAssign = i;
        } else if ((text == U"(") && (angleDepth == 0)) {
            idxParen = i;
            break;
        }
    }
    if ((idxParen == stmt.size()) || (idxAssign < idxParen)) {
        return false;
    }

    // The name before it
    size_t idxNameBegin = 0;
    size_t idxNameToken = 0;
    std::u32string simpleName;
    if (idxOperator < idxParen) {
        idxNameBegin = idxOperator;
        idxNameToken = idxOperator;
        for (size_t i = idxOperator; i < idxParen; i++) {
            simpleName += At(stmt, i).text;
        }
    } else {
        if ((idxParen == 0) || !IsIdentifier(stmt, idxParen - 1)) {
            // 'void (*fp)(int)', a lambda, a cast - not a function
            return false;
        }
        idxNameToken = idxParen - 1;
        idxNameBegin = idxNameToken;
        simpleName = At(stmt, idxNameToken).text;
        if ((idxNameBegin > 0) && IsText(stmt, idxNameBegin - 1, U"~")) {
            idxNameBegin--;
            simpleName = U"~" + simpleName;
        }
    }
    // The qualifier: 'A::B<T>::'
    std::vector<std::u32string> qualifierParts;
    while ((idxNameBegin >= 2) && IsText(stmt, idxNameBegin - 1, U"::")) {
        size_t i = idxNameBegin - 2;
        if (IsText(stmt, i, U">")) {
            int depth = 0;
            while (true) {
                depth += IsText(stmt, i, U">") ? 1 : IsText(stmt, i, U"<") ? -1 : 0;
                if ((depth == 0) || (i == 0)) {
                    break;
                }
                i--;
            }
            if (i == 0) {
                break;
            }
            i--;
        }
        if (!IsIdentifier(stmt, i)) {
            break;
        }
        qualifierParts.insert(qualifierParts.begin(), At(stmt, i).text);
        idxNameBegin = i;
    }
    std::u32string qualifier;
    for (auto &part : qualifierParts) {
        qualifier += qualifier.empty() ? part : U"::" + part;
    }

    // A type before the name - or a constructor / destructor / conversion operator, which have none
    bool hasType = false;
    bool isFriend = false;
    for (size_t i = 0; i < idxNameBegin; i++) {
        auto &text = At(stmt, i).text;
        if (text == U"friend") {
            isFriend = true;
        }
        if (!Contains(kSpecifiers, text) && (text != U"::")) {
            hasType = true;
        }
    }
    auto baseName = (simpleName[0] == U'~') ? simpleName.substr(1) : simpleName;
    bool isConstructor = (simpleName[0] == U'~') ||
                         (InClassScope() && qualifier.empty() && (baseName == CurrentScope().name)) ||
                         (!qualifier.empty() && (LastComponent(qualifier) == baseName));
    bool isOperator = (idxOperator < idxParen);
    if ((!hasType && !isConstructor && !isOperator) || isFriend) {
        return true;
    }
    auto kind = (InClassScope() && qualifier.empty()) ? kSymbolKind::kMethod : kSymbolKind::kFunction;
    outIdxSymbol = AddSymbol(kind, simpleName, qualifier, At(stmt, idxNameToken), isDefinition);
    return true;
}

// 'Type name [= ...], *other;' - members in a class (with a holds edge), variables elsewhere
void CPPStructureScanner::AddVariables(const Statement &stmt) {
    bool isExtern = false;
    size_t idxTypeBegin = 0;
    while ((idxTypeBegin < stmt.size()) && Contains(kSpecifiers, At(stmt, idxTypeBegin).text)) {
        isExtern = isExtern || IsText(stmt, idxTypeBegin, U"extern");
        idxTypeBegin++;
    }
    size_t idxTypeEnd = stmt.size();     // the first declarator's name

    size_t idxDeclarator = idxTypeBegin;
    bool isFirst = true;
    while (idxDeclarator < stmt.size()) {
        // The declarator ends at a top level ','
        size_t idxEnd = idxDeclarator;
        int depth = 0;
        for (; idxEnd < stmt.size(); idxEnd++) {
            auto &text = At(stmt, idxEnd).text;
            if ((text == U"(") || (text == U"[") || (text == U"<")) {
                depth++;
            } else if ((text == U")") || (text == U"]") || (text == U">")) {
                depth--;
            } else if (text == U">>") {
                depth -= 2;
            } else if ((text == U",") && (depth <= 0)) {
                break;
            }
        }
        // The name: '(*name)' for a function pointer, else the last identifier before '=', '[', ':', '}'
        size_t idxName = stmt.size();
        for (size_t i = idxDeclarator; i + 2 < idxEnd; i++) {
            if (IsText(stmt, i, U"(") && IsText(stmt, i + 1, U"*") && IsIdentifier(stmt, i + 2)) {
                idxName = i + 2;
                break;
            }
        }
        if (idxName == stmt.size()) {
            int angleDepth = 0;
            for (size_t i = idxDeclarator; i < idxEnd; i++) {
                auto &text = At(stmt, i).text;
                // 'std::function<void(int)> f' - only a top level '(' ends the name
                bool isEnd = (text == U"=") || (text == U"[") || (text == U"}") || (text == U"(") || (text == U":");
                if (isEnd && (angleDepth == 0)) {
                    break;
                }
                if (text == U"<") {
                    angleDepth++;
                } else if (text == U">") {
                    angleDepth--;
                } else if (text == U">>") {
                    angleDepth -= 2;
                } else if ((angleDepth == 0) && IsIdentifier(stmt, i) && !IsText(stmt, i + 1, U"::")) {
                    idxName = i;
                }
            }
        }
        if (idxName == stmt.size()) {
            return;
        }
        // An out-of-line static member: 'bool Runloop::bQuit = false;'
        size_t idxQualifierBegin = idxName;
        std::u32string qualifier;
        while ((idxQualifierBegin >= idxDeclarator + 2) && IsText(stmt, idxQualifierBegin - 1, U"::") && IsIdentifier(stmt, idxQualifierBegin - 2)) {
            idxQualifierBegin -= 2;
            qualifier = qualifier.empty() ? At(stmt, idxQualifierBegin).text : At(stmt, idxQualifierBegin).text + U"::" + qualifier;
        }
        if (isFirst) {
            // No type: 'MACRO;', 'name;' (e.g. after a class body)
            idxTypeEnd = idxQualifierBegin;
            if (idxTypeEnd == idxTypeBegin) {
                return;
            }
        }
        auto &nameToken = At(stmt, idxName);
        auto kind = InClassScope() ? kSymbolKind::kMember : kSymbolKind::kVariable;
        AddSymbol(kind, nameToken.text, qualifier, nameToken, !isExtern);
        if (kind == kSymbolKind::kMember) {
            // Later declarators share the type; their own '*' / '&' sits before their name
            bool isExtraPointer = false;
            for (size_t i = idxDeclarator; !isFirst && (i < idxName); i++) {
                isExtraPointer = isExtraPointer || IsText(stmt, i, U"*") || IsText(stmt, i, U"&");
            }
            AddHoldsEdge(CurrentScope().idxSymbol, stmt, idxTypeBegin, idxTypeEnd, isExtraPointer, nameToken.line);
        }
        isFirst = false;
        idxDeclarator = idxEnd + 1;
    }
}

void CPPStructureScanner::AddHoldsEdge(int idxFrom, const Statement &stmt, size_t idxBegin, size_t idxEnd, bool isExtraPointer, uint32_t line) {
    if (idxFrom < 0) {
        return;
    }
    std::vector<TypeToken> typeTokens;
    for (size_t i = idxBegin; i < idxEnd; i++) {
        auto &token = At(stmt, i);
        bool isBuiltin = (token.classification == kLanguageTokenClass::kKnownType) || (token.text == U"auto");
        // '>>' closes two template argument lists
        if (token.text == U">>") {
            typeTokens.push_back({U">"});
            typeTokens.push_back({U">"});
            continue;
        }
        typeTokens.push_back({token.text, isBuiltin});
    }
    size_t idx = 0;
    auto type = ParseType(typeTokens, idx);
    auto [target, ownership] = ResolveHeld(type, isExtraPointer);
    if (target.empty()) {
        return;
    }
    CodeEdge edge;
    edge.kind = kEdgeKind::kHolds;
    edge.idxFrom = idxFrom;
    edge.toName = target;
    edge.ownership = ownership;
    edge.line = line;
    result.edges.push_back(std::move(edge));
}

// 'template <...>', '[[attributes]]', 'alignas(...)' and a leading macro call 'DECLARE_X(Foo) void f()'
CPPStructureScanner::Statement CPPStructureScanner::StripPrefixes(const Statement &stmt) const {
    size_t idxBegin = 0;
    auto skipBalanced = [this, &stmt](size_t i, const char32_t *open, const char32_t *close) {
        int depth = 0;
        do {
            if (IsText(stmt, i, open)) {
                depth++;
            } else if (IsText(stmt, i, close)) {
                depth--;
            } else if ((std::u32string(close) == U">") && IsText(stmt, i, U">>")) {
                depth -= 2;
            }
            i++;
        } while ((i < stmt.size()) && (depth > 0));
        return i;
    };
    while (idxBegin < stmt.size()) {
        if (IsText(stmt, idxBegin, U"template") && IsText(stmt, idxBegin + 1, U"<")) {
            idxBegin = skipBalanced(idxBegin + 1, U"<", U">");
            continue;
        }
        if (IsText(stmt, idxBegin, U"[") && IsText(stmt, idxBegin + 1, U"[")) {
            idxBegin = skipBalanced(idxBegin, U"[", U"]");
            continue;
        }
        bool isAttribute = IsText(stmt, idxBegin, U"alignas") || IsText(stmt, idxBegin, U"__attribute__") || IsText(stmt, idxBegin, U"__declspec");
        bool isMacroCall = IsIdentifier(stmt, idxBegin) && IsAllCaps(At(stmt, idxBegin).text);
        if ((isAttribute || isMacroCall) && IsText(stmt, idxBegin + 1, U"(")) {
            auto idxAfter = skipBalanced(idxBegin + 1, U"(", U")");
            // A macro call is a prefix only when a declaration follows it
            if (isAttribute || ((idxAfter < stmt.size()) && !IsText(stmt, idxAfter, U"(") && !IsText(stmt, idxAfter, U"="))) {
                idxBegin = idxAfter;
                continue;
            }
        }
        break;
    }
    return Statement(stmt.begin() + (long)std::min(idxBegin, stmt.size()), stmt.end());
}

int CPPStructureScanner::AddSymbol(kSymbolKind kind, const std::u32string &name, const std::u32string &qualifier, const Token &at, bool isDefinition) {
    CodeSymbol symbol;
    symbol.name = name;
    auto prefix = ScopePrefix();
    if (!qualifier.empty()) {
        prefix = prefix.empty() ? qualifier : prefix + U"::" + qualifier;
    }
    // An anonymous type carries the name of the scope it's in
    symbol.qualifiedName = (prefix.empty() || name.empty()) ? prefix + name : prefix + U"::" + name;
    symbol.kind = kind;
    for (size_t i = scopes.size(); i > 0; i--) {
        if (scopes[i - 1].idxSymbol >= 0) {
            symbol.idxParent = scopes[i - 1].idxSymbol;
            break;
        }
    }
    symbol.line = at.line;
    symbol.column = at.column;
    symbol.beginLine = at.line;
    symbol.endLine = at.line;
    symbol.isDefinition = isDefinition;
    result.symbols.push_back(std::move(symbol));
    return (int)result.symbols.size() - 1;
}

// Namespaces and classes enclosing the current position; an enum class is a scope, a plain enum isn't
std::u32string CPPStructureScanner::ScopePrefix() const {
    std::u32string prefix;
    for (auto &scope : scopes) {
        bool isNamed = (scope.kind == kScope::kNamespace) || (scope.kind == kScope::kClass) || ((scope.kind == kScope::kEnum) && scope.isEnumClass);
        if (isNamed && !scope.name.empty()) {
            prefix += prefix.empty() ? scope.name : U"::" + scope.name;
        }
    }
    return prefix;
}

bool CPPStructureScanner::InClassScope() const {
    return !scopes.empty() && (scopes.back().kind == kScope::kClass);
}

bool CPPStructureScanner::IsText(const Statement &stmt, size_t idx, const char32_t *text) const {
    return (idx < stmt.size()) && (tokens[stmt[idx]].text == text);
}

bool CPPStructureScanner::IsIdentifier(const Statement &stmt, size_t idx) const {
    if (idx >= stmt.size()) {
        return false;
    }
    auto &token = tokens[stmt[idx]];
    if ((token.classification != kLanguageTokenClass::kRegular) || token.text.empty()) {
        return false;
    }
    auto ch = token.text[0];
    return ((ch >= U'a') && (ch <= U'z')) || ((ch >= U'A') && (ch <= U'Z')) || (ch == U'_') || (ch > 127);
}

//
// Name -> token class from what the lines declare - see SemanticNames.h
//

#include "SemanticNames.h"

using namespace gedit;

static kLanguageTokenClass TokenClassFromSymbolKind(kSymbolKind kind) {
    switch (kind) {
        case kSymbolKind::kNamespace :
            return kLanguageTokenClass::kNamespace;
        case kSymbolKind::kClass :
        case kSymbolKind::kStruct :
        case kSymbolKind::kUnion :
        case kSymbolKind::kEnum :
        case kSymbolKind::kAlias :
            return kLanguageTokenClass::kUserType;
        case kSymbolKind::kEnumerator :
            return kLanguageTokenClass::kEnumValue;
        case kSymbolKind::kFunction :
        case kSymbolKind::kMethod :
            return kLanguageTokenClass::kFunction;
        case kSymbolKind::kMember :
            return kLanguageTokenClass::kMember;
        case kSymbolKind::kMacro :
            return kLanguageTokenClass::kMacro;
        case kSymbolKind::kVariable :
            // A namespace-scope variable looks like any local - left as code for now
            return kLanguageTokenClass::kRegular;
    }
    return kLanguageTokenClass::kRegular;
}

std::optional<SemanticDeclaration> SemanticNames::DeclarationFor(const CodeSymbol &symbol) {
    auto tokenClass = TokenClassFromSymbolKind(symbol.kind);
    // Anonymous types, destructors ('~Foo' is two tokens to the lexer)
    if (symbol.name.empty() || (symbol.name[0] == U'~') || (tokenClass == kLanguageTokenClass::kRegular)) {
        return {};
    }
    return SemanticDeclaration{symbol.name, tokenClass};
}

SemanticNames SemanticNames::FromStructure(const FileStructure &structure) {
    SemanticNames semanticNames;
    for (auto &symbol : structure.symbols) {
        auto declaration = DeclarationFor(symbol);
        if (declaration.has_value()) {
            semanticNames.Add(*declaration);
        }
    }
    return semanticNames;
}

void SemanticNames::Add(const SemanticDeclaration &declaration) {
    if (!IsSemanticClass(declaration.tokenClass)) {
        return;
    }
    auto &counts = names[declaration.name];
    counts[(size_t)declaration.tokenClass - (size_t)kLanguageTokenClass::kUserType]++;
}

void SemanticNames::Remove(const SemanticDeclaration &declaration) {
    auto it = names.find(declaration.name);
    if ((it == names.end()) || !IsSemanticClass(declaration.tokenClass)) {
        return;
    }
    auto &count = it->second[(size_t)declaration.tokenClass - (size_t)kLanguageTokenClass::kUserType];
    if (count > 0) {
        count--;
    }
    for (auto remaining : it->second) {
        if (remaining > 0) {
            return;
        }
    }
    names.erase(it);
}

// A type wins (its constructors are methods of the same name); otherwise exactly one meaning, or none
kLanguageTokenClass SemanticNames::Lookup(const std::u32string &name) const {
    auto it = names.find(name);
    if (it == names.end()) {
        return kLanguageTokenClass::kRegular;
    }
    auto &counts = it->second;
    if (counts[0] > 0) {
        return kLanguageTokenClass::kUserType;
    }
    auto tokenClass = kLanguageTokenClass::kRegular;
    for (size_t i = 1; i < counts.size(); i++) {
        if (counts[i] == 0) {
            continue;
        }
        if (tokenClass != kLanguageTokenClass::kRegular) {
            return kLanguageTokenClass::kRegular;
        }
        tokenClass = (kLanguageTokenClass)((size_t)kLanguageTokenClass::kUserType + i);
    }
    return tokenClass;
}

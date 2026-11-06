//
// Semantic highlighting (docs/code-index.md §10.1, step 1b): name -> token class for the names a file
// declares. Counted per (name, class) - each line adds what it declares and takes it away again when it is
// rescanned or deleted - so an edit updates the table without a whole-file pass. Plain data: the text buffer
// looks up its 'regular' identifiers here after a line is parsed; the language layer never sees it.
//
// A name declared with two different meanings (a member here, a method there) stays 'regular' - never a
// guess. A constructor / destructor takes its class's meaning.
//

#ifndef GOATEDIT_SEMANTICNAMES_H
#define GOATEDIT_SEMANTICNAMES_H

#include <array>
#include <optional>
#include <string>
#include <unordered_map>

#include "FileStructure.h"
#include "LineStructure.h"
#include "Core/Language/LanguageTokenClass.h"

namespace gedit {
    class SemanticNames {
    public:
        SemanticNames() = default;
        virtual ~SemanticNames() = default;

        // What a symbol declares for highlighting - nothing for a nameless type or a plain variable
        static std::optional<SemanticDeclaration> DeclarationFor(const CodeSymbol &symbol);
        // A 'using' or an '#include' counts like a declaration, so an edit brings it and takes it away the same way
        static std::optional<SemanticDeclaration> DeclarationFor(const CodeEdge &edge);
        // kRegular for what has no colour of its own (a namespace-scope variable)
        static kLanguageTokenClass TokenClassFor(kSymbolKind kind);
        static SemanticNames FromStructure(const FileStructure &structure);
        static bool IsSemanticClass(kLanguageTokenClass tokenClass) {
            return (tokenClass >= kLanguageTokenClass::kUserType) && (tokenClass < kLanguageTokenClass::kLastTokenClass);
        }

        void Add(const SemanticDeclaration &declaration);
        void Remove(const SemanticDeclaration &declaration);
        void Clear() { names.clear(); }

        // kRegular when the name isn't known (or is ambiguous)
        kLanguageTokenClass Lookup(const std::u32string &name) const;
        // Declared here - also when Lookup gives kRegular because the meanings disagree
        bool Contains(const std::u32string &name) const { return names.find(name) != names.end(); }
        bool IsEmpty() const { return names.empty(); }
        // 'using std::vector;' is IsUsing(U"std::vector"), 'using namespace std;' is IsUsingNamespace(U"std")
        bool IsUsing(const std::u32string &qualifiedName) const;
        bool IsUsingNamespace(const std::u32string &scope) const;
        // As written, whichever quotes it had: IsIncluding(U"SDL3/SDL.h")
        bool IsIncluding(const std::u32string &header) const;
    protected:
        static constexpr size_t NUM_SEMANTIC_CLASSES = (size_t)kLanguageTokenClass::kLastTokenClass - (size_t)kLanguageTokenClass::kUserType;
        using Counts = std::array<uint32_t, NUM_SEMANTIC_CLASSES>;
    protected:
        std::unordered_map<std::u32string, Counts> names;
    };
}

#endif //GOATEDIT_SEMANTICNAMES_H

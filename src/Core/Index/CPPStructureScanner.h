//
// C/C++ structure pass (docs/code-index.md §4.2, step 1): the lexer's token stream -> namespaces, classes,
// structs, unions, enums (+ enumerators), functions/methods, data members, variables, aliases, macros, and
// the edges visible in a declaration: inherits, holds (with ownership), includes.
//
// Heuristic, not a parser: a scope stack and a statement buffer, classified at '{', ';' and '}'. Function
// bodies are skipped. Streaming, a line at a time: at a line that starts between statements the state is
// just the scope stack + conditionals, which is what a line-fed scan resumes from. Preprocessor: '#include' / '#define' are read, conditionals keep the first arm
// ('#if 0' keeps the '#else'), continuation lines are skipped. Comments and strings are dropped.
//

#ifndef GOATEDIT_CPPSTRUCTURESCANNER_H
#define GOATEDIT_CPPSTRUCTURESCANNER_H

#include <string>
#include <vector>

#include "StructureScanner.h"
#include "Core/Language/LangLineTokenizer.h"

namespace gedit {
    class CPPStructureScanner : public StructureScanner {
    public:
        // The C++ language's tokenizer (a copy - it has its own state stack)
        explicit CPPStructureScanner(const LangLineTokenizer &tokenizer);
        virtual ~CPPStructureScanner() = default;

        static Ptr Create(const LangLineTokenizer &tokenizer) {
            return std::make_unique<CPPStructureScanner>(tokenizer);
        }

        FileStructure Scan(const std::u32string &text) override;

        void Begin(const StructureScanState::Ref &state) override;
        StructureScanState::Ref StateAtLineStart() override;
        void ScanLine(const Line &line, uint32_t idxLine) override;
        FileStructure End() override;
    public:
        struct Token {
            std::u32string text;
            uint32_t line = 0;
            uint32_t column = 0;
            kLanguageTokenClass classification = kLanguageTokenClass::kUnknown;
        };
    protected:
        enum class kScope {
            kFile,
            kNamespace,
            kClass,
            kEnum,
            kExtern,        // extern "C" { - transparent
            kOpaque,        // a function body, or a block we don't look into
            kInitializer,   // braces inside a statement ('= {...}', 'a{1}') - the statement goes on after it
        };
        struct Scope {
            kScope kind = kScope::kFile;
            int idxSymbol = -1;
            std::u32string name;            // namespace / class name - for qualified names and constructors
            bool isEnumClass = false;
            bool isTypedef = false;         // 'typedef struct {...} Name;' - named at the ';'
            uint32_t openLine = 0;          // the line of its '{'
            kSymbolKind symbolKind = kSymbolKind::kClass;  // class / enum scopes - what a resumed scan names at the ';'

            // The resumable part - not the symbol (a resumed scan's symbols start over) or the line
            bool IsSameScope(const Scope &other) const {
                return (kind == other.kind) && (name == other.name) && (isEnumClass == other.isEnumClass) &&
                       (isTypedef == other.isTypedef) && (symbolKind == other.symbolKind);
            }
        };
        struct Conditional {
            bool isSkipping = false;
            bool isTaken = false;

            bool operator==(const Conditional &other) const {
                return (isSkipping == other.isSkipping) && (isTaken == other.isTaken);
            }
        };
        // The state at a line that starts between statements
        class ScanState : public StructureScanState {
        public:
            using Ref = std::shared_ptr<const ScanState>;
        public:
            bool Equals(const StructureScanState &other) const override;
            bool IsSame(const std::vector<Scope> &otherScopes, const std::vector<Conditional> &otherConditionals) const;
        public:
            std::vector<Scope> scopes;
            std::vector<Conditional> conditionals;
        };
        using Statement = std::vector<size_t>;     // indices into 'tokens'
    protected:
        void ScanLineTokens(std::vector<Token> &lineTokens);
        bool IsBetweenStatements() const;
        void ScanPreprocessorLine(const std::vector<Token> &lineTokens);
        bool IsSkipping() const;

        void ScanToken(size_t idxToken);
        void ScanTokenInScope(size_t idxToken);
        void OnOpenBrace(size_t idxToken);
        void OnCloseBrace(size_t idxToken);
        void OnSemicolon();
        void OnEnumItemEnd();

        void ClassifyDeclaration(Statement statement);
        bool TryClassDefinition(const Statement &statement, size_t idxBrace);
        bool TryEnumDefinition(const Statement &statement);
        bool TryFunction(const Statement &statement, bool isDefinition, int &outIdxSymbol);
        void AddVariables(const Statement &statement);
        void AddHoldsEdge(int idxFrom, const Statement &statement, size_t idxBegin, size_t idxEnd, bool isExtraPointer, uint32_t line);
        void OnTypedefName(const Statement &statement);
        void FlushLine(std::vector<Token> &lineTokens);

        Statement StripPrefixes(const Statement &statement) const;
        int AddSymbol(kSymbolKind kind, const std::u32string &name, const std::u32string &qualifier, const Token &at, bool isDefinition);
        std::u32string ScopePrefix() const;
        Scope &CurrentScope() { return scopes.back(); }
        bool InClassScope() const;

        const Token &At(const Statement &statement, size_t idx) const { return tokens[statement[idx]]; }
        bool IsText(const Statement &statement, size_t idx, const char32_t *text) const;
        bool IsIdentifier(const Statement &statement, size_t idx) const;
    protected:
        LangLineTokenizer tokenizer;

        // Per scan
        FileStructure result;
        std::vector<Token> tokens;      // code tokens: no comments, strings or preprocessor lines
        std::vector<Scope> scopes;
        Statement statement;
        Statement enumItem;
        int parenDepth = 0;             // in 'statement'
        int pendingTypedefSymbol = -1;  // 'typedef struct/enum {...} Name;' - the type waiting for its name
        bool isAfterAnonymousType = false;  // 'struct {...} name;' - the names before the ';' are members
        bool isPendingTypedefAlias = false; // a resumed scan at 'typedef struct Foo {...} Foo_t;' - the alias at the ';'
        std::vector<Conditional> conditionals;
        ScanState::Ref lastState = nullptr;   // shared by the following lines while unchanged
        bool isContinuation = false;
        uint32_t idxPreviousLine = 0;   // the last line with tokens - a gap is an empty line, which ends a continuation
    };
}

#endif //GOATEDIT_CPPSTRUCTURESCANNER_H

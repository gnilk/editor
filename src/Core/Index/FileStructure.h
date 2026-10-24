//
// The structure of one source file as the code index sees it (docs/code-index.md §4.2): symbols and the
// relationship edges visible in the file itself. Plain data - produced by a StructureScanner, consumed by
// the index. Names in edges are as written; resolving them to symbols is the index's job (§5.2).
//

#ifndef GOATEDIT_FILESTRUCTURE_H
#define GOATEDIT_FILESTRUCTURE_H

#include <cstdint>
#include <string>
#include <vector>

namespace gedit {
    enum class kSymbolKind : uint8_t {
        kNamespace,
        kClass,
        kStruct,
        kUnion,
        kEnum,
        kEnumerator,
        kFunction,      // free function, or an out-of-line definition 'A::f' (A may be a class or a namespace)
        kMethod,        // declared/defined inside a class body
        kMember,        // data member
        kVariable,      // namespace-scope variable
        kAlias,         // using X = ... / typedef
        kMacro,         // #define
    };

    enum class kEdgeKind : uint8_t {
        kInherits,      // class -> base
        kHolds,         // class -> type of a data member (see kOwnership)
        kIncludes,      // file -> included path
    };

    enum class kOwnership : uint8_t {
        kNone,
        kOwns,          // by value, unique_ptr, a container of either
        kShares,        // shared_ptr, weak_ptr, a 'Ref' alias
        kBorrows,       // raw pointer or reference
    };

    struct CodeSymbol {
        std::u32string name;            // 'SearchFor'
        std::u32string qualifiedName;   // enclosing scopes + qualifier as written: 'gedit::Document::SearchFor'
        kSymbolKind kind = kSymbolKind::kFunction;
        int idxParent = -1;             // enclosing symbol in this file (-1: file scope)
        uint32_t line = 0;              // 0-based, where the name is
        uint32_t column = 0;            // char index of the name
        uint32_t beginLine = 0;         // first line of the definition - before 'line' for 'typedef struct {...} Name;'
        uint32_t endLine = 0;           // last line of the body, or 'line' when there is none
        bool isDefinition = false;      // has a body (class/function) - false for declarations / forward declarations
    };

    struct CodeEdge {
        kEdgeKind kind = kEdgeKind::kInherits;
        int idxFrom = -1;               // symbol (-1: the file itself, e.g. includes)
        std::u32string toName;          // as written, template arguments stripped: 'ViewBase', 'gedit::Line'
        kOwnership ownership = kOwnership::kNone;
        uint32_t line = 0;
        bool isSystemInclude = false;   // kIncludes: <...> rather than "..."
    };

    struct FileStructure {
        std::vector<CodeSymbol> symbols;
        std::vector<CodeEdge> edges;
        size_t numLines = 0;

        // Brace balance at the end of the file. A mismatch usually means the lexer misread something - or
        // the first-arm '#if' policy cut a construct in half
        uint32_t numUnclosedBraces = 0;
        uint32_t lineFirstUnclosed = 0;     // where the outermost still-open brace was opened
        uint32_t numStrayCloseBraces = 0;
        uint32_t lineFirstStrayClose = 0;
    };

    const char *SymbolKindName(kSymbolKind kind);
    const char *EdgeKindName(kEdgeKind kind);
    const char *OwnershipName(kOwnership ownership);
}

#endif //GOATEDIT_FILESTRUCTURE_H

//
// The in-memory code index (docs/code-index.md §4.3, §4.5): the symbols of every indexed file and a ranked
// name search over them. Plain data in, plain data out - no Editor, no UI, no threads of its own. Safe to
// use from several threads: files are replaced one at a time under a short write lock, a query takes a
// read lock, so a search during indexing sees whatever is indexed so far.
//
// Each distinct name is stored once with a mask of the characters in it. A query walks the names, not the
// symbols: the mask rejects most of them without touching the text, and the rest are scored. That is what
// keeps a keypress cheap - and unlike a trigram table it also works for subsequences ('srch' -> 'SearchFor').
//

#ifndef GOATEDIT_CODEINDEX_H
#define GOATEDIT_CODEINDEX_H

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "FileStructure.h"
#include "WorkspaceNames.h"

namespace gedit {
    class CodeIndex {
    public:
        using Ref = std::shared_ptr<CodeIndex>;

        struct Hit {
            std::u32string name;
            std::u32string qualifiedName;
            std::string path;
            kSymbolKind kind = kSymbolKind::kFunction;     // an out-of-line 'A::f' is a method when the index knows A as a class
            uint32_t line = 0;
            uint32_t column = 0;
            bool isDefinition = false;
        };

        // An edge between two names (§5.1). Plain data, so a view, a command or an agent can ask alike (§10.3).
        struct Relation {
            kEdgeKind kind = kEdgeKind::kInherits;
            kOwnership ownership = kOwnership::kNone;
            std::u32string fromName;
            std::u32string toName;                  // as written
            // What 'toName' can mean from where it is written (§5.2), qualified. None: outside the index ('gnilk::ILogger').
            // Several: ambiguous, and all are shown - never a guess.
            std::vector<std::u32string> targets;
            bool isMacro = false;                   // the target is '#define _u8 unsigned char', no type has the name
            std::string path;
            uint32_t line = 0;
        };

        struct QueryContext {
            std::string activePath;     // what closeness is measured from - empty: no proximity ranking
            size_t maxHits = 200;
        };

        struct Stats {
            size_t numFiles = 0;
            size_t numSymbols = 0;
            size_t numNames = 0;
            size_t numScopes = 0;
            size_t numEdges = 0;
            size_t numBytes = 0;        // estimate of what the tables hold
        };

        // How well a query matches a name - best last. kNone is 'no match'
        enum class kMatchQuality : uint8_t {
            kNone,
            kSubsequence,   // 'srch' -> 'SearchFor'
            kSubstring,     // 'arch' -> 'SearchFor'
            kWordStarts,    // 'SF' / 'sefo' -> 'SearchFor'
            kPrefix,        // 'Sea' -> 'SearchFor'
            kExact,
        };
    public:
        CodeIndex() = default;
        virtual ~CodeIndex() = default;

        static Ref Create() {
            return std::make_shared<CodeIndex>();
        }

        // Case-insensitive; 'lowerQuery' and 'lowerName' are the lower-cased forms, 'name' the original
        static kMatchQuality MatchName(const std::u32string_view &lowerQuery, const std::u32string_view &name, const std::u32string_view &lowerName);

        // A file's symbols, all of them, replacing what the file had
        void ReplaceFile(const std::string &path, const FileStructure &structure);
        void RemoveFile(const std::string &path);

        // 'Name', or 'Scope::Name' where the part before the last '::' must appear in the symbol's scope.
        // Best first - match quality, then kind, then closeness to the active file.
        std::vector<Hit> Query(const std::u32string &query, const QueryContext &context) const;
        std::vector<Hit> Query(const std::u32string &query) const {
            return Query(query, QueryContext{});
        }
        // Every symbol with exactly this name, case and all, unranked
        std::vector<Hit> FindExact(const std::u32string &name) const;
        // The last symbol at or above the line, namespaces aside. Not the enclosing one: an open file's symbols have no
        // extent in the index, see TextBuffer::CollectStructure.
        std::optional<Hit> NearestSymbol(const std::string &path, uint32_t line) const;
        static std::u32string LowerCase(const std::u32string_view &text);

        // The types this name can be, qualified: 'Line' or 'gedit::Line' -> 'gedit::Line'. The relations below take these.
        std::vector<std::u32string> TypesNamed(const std::u32string &name) const;
        // One level each - a tree asks again per row, so a cycle is one more row and not a loop. By name, then line.
        std::vector<Relation> BasesOf(const std::u32string &qualifiedType) const;
        std::vector<Relation> DerivedFrom(const std::u32string &qualifiedType) const;
        std::vector<Relation> HoldsOf(const std::u32string &qualifiedType) const;
        std::vector<Relation> HeldBy(const std::u32string &qualifiedType) const;
        // Every 'inherits' and 'holds' edge, resolved: the whole structure at once, to compare or to measure
        std::vector<Relation> AllRelations() const;

        // What each name means across the index. The same pointer until a meaning changes, so a caller can
        // compare pointers to see if there is anything to recolour. Names with several meanings aren't in it.
        WorkspaceNames::Ref GetWorkspaceNames();
        Stats GetStats() const;
    private:
        struct Symbol {
            uint32_t idName = 0;
            uint32_t idScope = 0;
            uint32_t line = 0;
            uint32_t column = 0;
            kSymbolKind kind = kSymbolKind::kFunction;
            bool isDefinition = false;
        };

        struct File {
            std::string path;
            std::vector<Symbol> symbols;
            std::vector<CodeEdge> edges;
        };

        struct SymbolHandle {
            uint32_t idFile = 0;
            uint32_t idxSymbol = 0;
        };

        struct EdgeHandle {
            uint32_t idFile = 0;
            uint32_t idxEdge = 0;
        };

        struct Name {
            std::u32string text;
            std::u32string lower;
            uint64_t charMask = 0;
            std::vector<SymbolHandle> symbols;
        };

        struct Scope {
            std::u32string text;
            std::u32string lower;
        };

        struct Candidate {
            kMatchQuality quality = kMatchQuality::kNone;
            uint8_t kindRank = 0;
            uint8_t proximity = 0;
            bool isDefinition = false;
            uint32_t nameLength = 0;
            SymbolHandle handle = {};
        };
    private:
        // Caller holds the write lock
        void RemoveSymbols(uint32_t idFile);
        void AddEdges(uint32_t idFile, const FileStructure &structure);
        void RemoveEdges(uint32_t idFile);
        uint32_t InternName(const std::u32string &text);
        uint32_t InternScope(const std::u32string &text);

        std::vector<bool> MatchingScopes(const std::u32string &lowerScopeQuery) const;
        uint8_t Proximity(const File &file, const std::string &activeFolder, const std::string &activePath) const;
        Hit MakeHit(const SymbolHandle &handle) const;
        bool IsClassScope(const std::u32string &scope) const;
        std::vector<Relation> EdgesFrom(const std::u32string &qualifiedType, kEdgeKind kind) const;
        std::vector<Relation> EdgesTo(const std::u32string &qualifiedType, kEdgeKind kind) const;
        Relation MakeRelation(const EdgeHandle &handle) const;
        std::vector<std::u32string> TypesWithLastName(const std::u32string &lastName) const;
        std::vector<std::u32string> ResolveTarget(const std::u32string &toName, const std::u32string &fromName, bool &outIsMacro) const;
        bool IsMacro(const std::u32string &name) const;
        std::optional<kLanguageTokenClass> SingleMeaning(const Name &name) const;
        bool IsBetter(const Candidate &a, const Candidate &b) const;
    private:
        mutable std::shared_mutex lock;

        // deques: the lookup maps hold views into the text, so an entry may never move
        std::deque<File> files;
        std::deque<Name> names;
        std::deque<Scope> scopes;
        std::unordered_map<std::string, uint32_t> fileIds;
        std::unordered_map<std::u32string_view, uint32_t> nameIds;
        std::unordered_map<std::u32string_view, uint32_t> scopeIds;
        std::vector<uint32_t> freeNames;        // entries whose last symbol went away
        // Apart from 'names': many targets are never symbols ('gnilk::ILogger'), and a search shouldn't walk them
        std::unordered_map<std::u32string, std::vector<EdgeHandle>> edgesByFrom;       // the qualified name
        std::unordered_map<std::u32string, std::vector<EdgeHandle>> edgesByTarget;     // the last component, as 'A::Line' and 'Line' must meet
        size_t numSymbols = 0;
        size_t numEdges = 0;
        uint64_t generation = 0;                // counts the changes to the files

        // Taken before 'lock'
        std::mutex workspaceNamesLock;
        WorkspaceNames::Ref workspaceNames = nullptr;
        uint64_t generationOfNames = 0;
    };
}

#endif //GOATEDIT_CODEINDEX_H

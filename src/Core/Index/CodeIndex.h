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
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "FileStructure.h"

namespace gedit {
    class CodeIndex {
    public:
        using Ref = std::shared_ptr<CodeIndex>;

        struct Hit {
            std::u32string name;
            std::u32string qualifiedName;
            std::string path;
            kSymbolKind kind = kSymbolKind::kFunction;
            uint32_t line = 0;
            uint32_t column = 0;
            bool isDefinition = false;
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
        };

        struct SymbolHandle {
            uint32_t idFile = 0;
            uint32_t idxSymbol = 0;
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
        uint32_t InternName(const std::u32string &text);
        uint32_t InternScope(const std::u32string &text);

        std::vector<bool> MatchingScopes(const std::u32string &lowerScopeQuery) const;
        uint8_t Proximity(const File &file, const std::string &activeFolder, const std::string &activePath) const;
        Hit MakeHit(const SymbolHandle &handle) const;
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
        size_t numSymbols = 0;
    };
}

#endif //GOATEDIT_CODEINDEX_H

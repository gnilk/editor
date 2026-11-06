//
// Back and forward through jumps (docs/code-index.md §5.6): every go-to, not every cursor move. Plain data - what a
// jump means to a document is the caller's.
//
// An entry holds the nearest symbol at or above its line and the distance to it, so lines coming and going above it
// don't lose the place. With no symbol - no index, a file it doesn't hold, the symbol gone - the line is what's left.
//
// It works like a browser's history, with two exceptions: a jump within a symbol replaces the place you are at, and a
// jump to where back would go is a back, so bouncing between a declaration and its definition doesn't grow the stack.
//

#ifndef GOATEDIT_NAVIGATIONSTACK_H
#define GOATEDIT_NAVIGATIONSTACK_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "CodeIndex.h"
#include "Core/Session/SessionState.h"

namespace gedit {
    class NavigationStack {
    public:
        struct Entry {
            std::string path;
            std::u32string symbol;      // qualified - empty: only the line
            int32_t lineOffset = 0;     // from the symbol's line
            uint32_t line = 0;
            uint32_t column = 0;
        };
    public:
        NavigationStack() = default;
        virtual ~NavigationStack() = default;

        // 'index' may be nullptr
        static Entry MakeEntry(const CodeIndex *index, const std::string &path, uint32_t line, uint32_t column);
        // Where the entry's place is now
        static uint32_t CurrentLine(const CodeIndex *index, const Entry &entry);

        // A jump from 'from' to 'to'. An empty 'from.path' is a place that can't be gone back to, like a new document.
        void Jump(const Entry &from, const Entry &to);
        // 'current' is kept, so the opposite direction comes back to it
        std::optional<Entry> Back(const Entry &current);
        std::optional<Entry> Forward(const Entry &current);

        void Clear();
        size_t Size() const { return entries.size(); }
        size_t CurrentIndex() const { return idxCurrent; }

        NavigationSession ToSession() const;
        void FromSession(const NavigationSession &session);
    protected:
        static constexpr size_t MAX_ENTRIES = 100;
    protected:
        std::vector<Entry> entries;
        size_t idxCurrent = 0;
    };
}

#endif //GOATEDIT_NAVIGATIONSTACK_H

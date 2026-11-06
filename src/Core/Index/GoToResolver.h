//
// Smart go-to (docs/code-index.md §5.6): where the name under the cursor can lead. A name is a name (§5.2), so this
// narrows by what the code says and never guesses: what's left is either one place or a list to choose from, best first.
//
// Each narrowing step keeps what it had if it would leave nothing: 'Alias::run' finding no 'Alias' in any scope is still
// better answered with every 'run'.
//

#ifndef GOATEDIT_GOTORESOLVER_H
#define GOATEDIT_GOTORESOLVER_H

#include <cstdint>
#include <string>
#include <vector>

#include "CodeIndex.h"

namespace gedit {
    class GoToResolver {
    public:
        enum class kMode {
            kDefinition,
            kDeclaration,
            kToggle,        // on one of the two, the other one - anywhere else, the definition
        };

        struct Result {
            std::u32string name;                // what's under the cursor, empty if nothing is
            size_t idxName = 0;
            std::vector<CodeIndex::Hit> hits;   // best first
            bool isAlreadyThere = false;        // the only place it can lead is under the cursor
        };
    public:
        // 'text' is the line 'idxLine' of 'path'
        static Result Resolve(const CodeIndex &index, const std::u32string &text, size_t idxChar, uint32_t idxLine, const std::string &path, kMode mode);
    };
}

#endif //GOATEDIT_GOTORESOLVER_H

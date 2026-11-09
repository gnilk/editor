//
// Smart go-to (docs/code-index.md §5.6): where the name under the cursor can lead. A name is a name (§5.2), so this
// narrows by what the code says and never guesses: what's left is either one place or a list to choose from, best first.
//
// 'a.b' and 'a->b' follow the types on symbols (§11.11): from 'this', a member or a global, link by link. A local, a
// parameter or 'auto' ends the chain, and the name is narrowed as it was without one.
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
            // 'lines.size()': the receiver is a library type ('std::vector'), so the project's 'size' isn't meant - no hits
            std::u32string libraryType;
        };

        enum class kRelated {
            kImplementation,    // a class: the types derived from it. A method: its overrides, at any depth.
            kBase,              // a class: its bases. A method: the nearest one it overrides.
        };
    public:
        // 'text' is the line 'idxLine' of 'path'
        static Result Resolve(const CodeIndex &index, const std::u32string &text, size_t idxChar, uint32_t idxLine, const std::string &path, kMode mode);
        // From the type or method under the cursor, else the one the cursor is in. A name is a name (§5.2): a method overrides
        // any method of that name in a base, so overloads aren't told apart.
        static Result ResolveRelated(const CodeIndex &index, const std::u32string &text, size_t idxChar, uint32_t idxLine, const std::string &path, kRelated related);
        // The type of the member, variable or function under the cursor (§11.11): what '.' reaches, a container's element.
        // On a type, the type itself.
        static Result ResolveTypeOf(const CodeIndex &index, const std::u32string &text, size_t idxChar, uint32_t idxLine, const std::string &path);
    };
}

#endif //GOATEDIT_GOTORESOLVER_H

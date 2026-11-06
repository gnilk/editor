//
// Semantic highlighting's second source (docs/code-index.md §10.1, §11.5): what a name means across the
// workspace, for names a file doesn't declare itself. An immutable snapshot of the code index - a parse thread
// takes the pointer once and probes it per identifier, no lock - so a new meaning is a new snapshot.
//
// The index holds no locals, so a name alone can't always say what it is: every class with a member 'path' would
// colour every local 'path' in the workspace. Members, functions and anything starting in lower case need the code
// around them too. Plain is cheap, a wrong colour isn't.
//

#ifndef GOATEDIT_WORKSPACENAMES_H
#define GOATEDIT_WORKSPACENAMES_H

#include <memory>
#include <string>
#include <unordered_map>

#include "Core/Language/LanguageTokenClass.h"

namespace gedit {
    class WorkspaceNames {
    public:
        using Ref = std::shared_ptr<const WorkspaceNames>;
    public:
        WorkspaceNames() = default;
        virtual ~WorkspaceNames() = default;

        // What the code itself says, whatever the name means elsewhere: after '.' or '->' it is a method when
        // called, otherwise a member. kRegular when the syntax can't tell. C and C++ only, as the index is.
        static kLanguageTokenClass ClassFromSyntax(const std::u32string &text, size_t idxName, size_t idxEnd);

        // 'tokenClass' if the code around the name at 'idxName' allows it, otherwise kRegular: a function must be
        // called, a member must be reached through '.' or '->', and a lower-case type, macro, enum value or namespace
        // must be where a local can't be
        static kLanguageTokenClass ClassInContext(const std::u32string &text, size_t idxName, const std::u32string &name, kLanguageTokenClass tokenClass);

        void Add(const std::u32string &name, kLanguageTokenClass tokenClass);

        // kRegular when the workspace gives the name no single meaning
        // Declared in the project - also when Lookup gives kRegular because it has no single meaning
        bool Contains(const std::u32string &name) const { return names.find(name) != names.end(); }
        kLanguageTokenClass Lookup(const std::u32string &name) const;
        // The same for the name at 'idxName' in a line - see ClassInContext
        kLanguageTokenClass LookupAt(const std::u32string &text, size_t idxName, const std::u32string &name) const;

        size_t Size() const { return names.size(); }
        bool operator==(const WorkspaceNames &other) const { return names == other.names; }
    protected:
        std::unordered_map<std::u32string, kLanguageTokenClass> names;
    };
}

#endif //GOATEDIT_WORKSPACENAMES_H

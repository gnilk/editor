//
// Semantic highlighting's third source (docs/code-index.md §10.2): what a program gets from its platform. Whatever is
// written 'std::' is the standard library by syntax alone; the rest comes from name lists shipped as assets, so nothing
// here knows the installed toolchain. A bare name is only library when the file can see it: C names always, the C++
// ones under a 'using'.
//
// An immutable snapshot, built at startup - parse threads probe it per identifier, no lock.
//

#ifndef GOATEDIT_LIBRARYNAMES_H
#define GOATEDIT_LIBRARYNAMES_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "SemanticNames.h"
#include "Core/Language/LanguageTokenClass.h"

namespace gedit {
    class LibraryNames {
    public:
        using Ref = std::shared_ptr<const LibraryNames>;
    public:
        LibraryNames() = default;
        virtual ~LibraryNames() = default;

        // The lists that always apply. A list that won't load leaves its names plain, and says so in the log.
        static Ref LoadShipped();
        // True for the name at 'idxName' in 'std::x', '::std::x', 'std::chrono::x'
        static bool IsQualifiedStd(const std::u32string &text, size_t idxName);

        // One list as YAML - see Assets/Resources/Libraries/c_std.yml. False, and nothing added, if it's malformed.
        bool Add(const std::string &yaml);

        // A name IsQualifiedStd says is library. One the lists don't have goes by what follows it.
        kLanguageTokenClass LookupQualified(const std::u32string &text, size_t idxName, const std::u32string &name) const;
        // A name with no 'std::': kRegular unless the file can see it, and the code around it allows it
        kLanguageTokenClass LookupBare(const std::u32string &text, size_t idxName, const std::u32string &name, const SemanticNames &fileNames) const;

        size_t Size() const { return names.size(); }
    protected:
        struct Entry {
            kLanguageTokenClass tokenClass = kLanguageTokenClass::kRegular;
            std::vector<std::u32string> scopes;     // 'std', 'std::chrono', empty for a C name
        };
    protected:
        std::unordered_map<std::u32string, Entry> names;
    };
}

#endif //GOATEDIT_LIBRARYNAMES_H

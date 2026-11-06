//
// Semantic highlighting's third source (docs/code-index.md §10.2): what a program gets from its platform. Whatever is
// written 'std::' is the standard library by syntax alone; the rest comes from name lists - shipped as assets, or a
// project's own - so nothing here knows the installed toolchain. A name is only library when the file can see it: its
// list must apply (one naming headers needs the file to include one), and a name in a scope must be written with that
// scope or brought in by a 'using'.
//
// An immutable snapshot, built at startup - parse threads probe it per identifier, no lock.
//

#ifndef GOATEDIT_LIBRARYNAMES_H
#define GOATEDIT_LIBRARYNAMES_H

#include <filesystem>
#include <memory>
#include <optional>
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

        // The shipped lists, then 'projectDir'/libraries/*.yml. A list that won't load leaves its names plain, and says
        // so in the log.
        static Ref Load(const std::optional<std::filesystem::path> &projectDir);
        // True for the name at 'idxName' in 'std::x', '::std::x', 'std::chrono::x'
        static bool IsQualifiedStd(const std::u32string &text, size_t idxName);

        // 'std::chrono' for the name at 'idxName' in 'std::chrono::x'. Empty if there is none, or template arguments hide it.
        static std::u32string WrittenQualifier(const std::u32string &text, size_t idxName);

        // One list as YAML - see Assets/Resources/Libraries/c_std.yml. False, and nothing added, if it's malformed.
        bool Add(const std::string &yaml);

        // A name IsQualifiedStd says is library. One the lists don't have goes by what follows it.
        kLanguageTokenClass LookupQualified(const std::u32string &text, size_t idxName, const std::u32string &name) const;
        // A name not written 'std::': kRegular unless the file can see it, and the code around it allows it
        kLanguageTokenClass LookupBare(const std::u32string &text, size_t idxName, const std::u32string &name, const SemanticNames &fileNames) const;

        size_t Size() const { return names.size(); }
    protected:
        // A name in one list, under one scope
        struct Origin {
            kLanguageTokenClass tokenClass = kLanguageTokenClass::kRegular;
            std::u32string scope;                   // 'std', 'std::chrono', empty for a C name
            size_t idxList = 0;
        };
        using Headers = std::vector<std::u32string>;     // what makes a list apply - empty: it always does
    protected:
        static bool IsApplying(const Headers &headers, const SemanticNames &fileNames);
    protected:
        std::unordered_map<std::u32string, std::vector<Origin>> names;
        std::vector<Headers> lists;
    };
}

#endif //GOATEDIT_LIBRARYNAMES_H

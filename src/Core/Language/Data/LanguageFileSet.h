//
// The language files (docs/support-languages.md §2.2), loaded and resolved: a file names its parent with
// 'inherit: <name>' and gets everything it doesn't set from it. Resolved once per name and kept, as
// KeyMappingCache does for keymaps.
//
// Merge rules (not YAMLMerge's - that one merges lists by position, open-bugs #28):
//   - a scalar or a map key from the child wins; maps merge key by key;
//   - a list from the child REPLACES the parent's list whole;
//   - '<key>+: [...]' appends to the parent's list instead.
//
// 'config.yml' 'languages.default' is laid over the root file 'default' (so a global 'tabsize' reaches every
// language that has no convention of its own); 'languages.<name>' is the language's business (DataLanguage).
//

#ifndef GEDIT_LANGUAGEFILESET_H
#define GEDIT_LANGUAGEFILESET_H

#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace gedit {
    class LanguageFileSet {
    public:
        using Ref = std::shared_ptr<LanguageFileSet>;

        struct Problem {
            std::string origin;     // the file (or the name given to a string)
            int line = 0;           // 1-based, 0 = unknown
            std::string reason;
        };

        static constexpr const char *DEFAULT_NAME = "default";
    public:
        static Ref Create() {
            return std::make_shared<LanguageFileSet>();
        }

        // Add one file's text. Its name is 'language.name'. A later file of the same name replaces an earlier one
        // (the user's over the system's). Returns false (and records a problem) if it can't be read.
        bool AddFromString(const std::string &yamlText, const std::string &origin);
        // Every '*.yml' in the folder, in name order. Returns how many were added.
        size_t AddFromDirectory(const std::filesystem::path &directory);

        // 'languages.default' from config.yml, laid over the root file 'default' when it is resolved.
        void SetDefaultOverrides(const YAML::Node &newDefaultOverrides);

        // The file with its 'inherit' chain folded in, without the 'inherit' key. Undefined if the name is
        // unknown or its chain is broken (unknown parent, cycle) - the reason is in Problems().
        YAML::Node Resolve(const std::string &name);

        std::vector<std::string> Names() const;
        bool Has(const std::string &name) const;
        std::string OriginOf(const std::string &name) const;
        const std::vector<Problem> &Problems() const { return problems; }

        // The merge on its own: 'child' laid over 'base'. A new tree - neither input is changed; the leaves and
        // lists are shared with the inputs (so a node's Mark() still names its line in the file).
        static YAML::Node Merge(const YAML::Node &base, const YAML::Node &child);

    private:
        struct File {
            std::string origin;
            YAML::Node root;
        };
    private:
        YAML::Node ResolveChain(const std::string &name, std::set<std::string> &inProgress);
        static YAML::Node WithoutInherit(const YAML::Node &root);
        void AddProblem(const std::string &origin, int line, const std::string &reason);
    private:
        std::map<std::string, File> files;
        std::map<std::string, YAML::Node> resolved;
        YAML::Node defaultOverrides;
        std::vector<Problem> problems;
    };
}

#endif //GEDIT_LANGUAGEFILESET_H

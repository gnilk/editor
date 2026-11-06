//
// What a project root leaves out, the one owner of that question (docs/code-index.md §11.7). Two kinds, kept
// apart because they mean different things:
//
//  - excluded: 'workspace.exclude', names and globs for what nothing should ever touch ('.git')
//  - ignored:  the repository's '.gitignore' files - not source, but it exists and can be browsed
//
// Git's own rules: '#' comments, '!' negation, a trailing '/' for folders only, a '/' at the start or in the
// middle anchors a pattern to its file's folder, '*' '?' '[...]' within one path component, '**' across them.
// The last matching line wins, a deeper file beats a shallower one, and nothing below an ignored folder can
// come back. The user's global ignore file sits below all of them, as in git. Not read: '.git/info/exclude', and
// a global file named by 'core.excludesFile' - that takes parsing git's config.
//
// Each folder's '.gitignore' is read on first need and kept, so a walk costs one stat per folder. Safe to use
// from several threads. Plain data in, plain verdict out: no editor types, no singleton.
//

#ifndef GOATEDIT_EXCLUSIONRULES_H
#define GOATEDIT_EXCLUSIONRULES_H

#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "FsFilter.h"

namespace gedit {
    class ExclusionRules {
    public:
        using Ref = std::shared_ptr<ExclusionRules>;

        enum class kVerdict : uint8_t {
            kIncluded,
            kIgnored,
            kExcluded,
        };

        struct Pattern {
            std::vector<std::string> segments;      // split on '/', a '**' segment spans any number of components
            bool isNegated = false;
            bool isDirectoryOnly = false;
            bool isAnchored = false;
        };
    public:
        // 'globalIgnoreFile': see DefaultGlobalIgnoreFile. Empty: none.
        ExclusionRules(const std::filesystem::path &root, std::vector<std::string> excludePatterns, bool useGitIgnore,
                       const std::filesystem::path &globalIgnoreFile = {});
        virtual ~ExclusionRules() = default;

        static Ref Create(const std::filesystem::path &root, std::vector<std::string> excludePatterns, bool useGitIgnore,
                          const std::filesystem::path &globalIgnoreFile = {}) {
            return std::make_shared<ExclusionRules>(root, std::move(excludePatterns), useGitIgnore, globalIgnoreFile);
        }

        // Where git looks by default: '$XDG_CONFIG_HOME/git/ignore', or '~/.config/git/ignore'
        static std::filesystem::path DefaultGlobalIgnoreFile();

        // One line of a '.gitignore'; false for a blank line or a comment
        static bool ParseLine(const std::string &line, Pattern &outPattern);
        // 'relPath' is relative to the folder of the pattern's file
        static bool Matches(const Pattern &pattern, const std::vector<std::string> &relPath, bool isDirectory);

        // Anything outside the root is included. 'isDirectory' for the last component - the ones above it are
        // folders by being above it.
        kVerdict Check(const std::filesystem::path &path, bool isDirectory) const;
        bool IsIncluded(const std::filesystem::path &path, bool isDirectory) const {
            return Check(path, isDirectory) == kVerdict::kIncluded;
        }
        // 'workspace.exclude' only - no '.gitignore' to evaluate, for a walk that keeps what is ignored
        bool IsExcluded(const std::filesystem::path &path) const;
        // A folder changed: its '.gitignore' may have
        void Invalidate(const std::filesystem::path &folder);
        // Events were lost: any of them may have
        void InvalidateAll();
    private:
        using Patterns = std::vector<Pattern>;
    private:
        bool RelativeComponents(const std::filesystem::path &path, const std::filesystem::path &base, std::vector<std::string> &outComponents) const;
        bool ComponentsBelowRoot(const std::filesystem::path &path, std::vector<std::string> &outComponents) const;
        bool IsAnyExcluded(const std::vector<std::string> &components) const;
        std::filesystem::path FindRepositoryTop() const;
        std::shared_ptr<const Patterns> PatternsForFolder(const std::filesystem::path &folder) const;
        static Patterns LoadPatterns(const std::filesystem::path &file);
        bool IsIgnored(const std::filesystem::path &top, const std::vector<std::string> &components, bool isDirectory) const;
    private:
        std::filesystem::path root;
        std::filesystem::path canonicalRoot;
        std::filesystem::path repositoryTop;    // where the '.gitignore' files start - can be above the root
        FsFilter excludeFilter;
        bool useGitIgnore = true;
        Patterns globalPatterns;                // relative to the repository top

        mutable std::shared_mutex cacheLock;
        mutable std::unordered_map<std::string, std::shared_ptr<const Patterns>> cache;     // folder -> its file
    };
}

#endif //GOATEDIT_EXCLUSIONRULES_H

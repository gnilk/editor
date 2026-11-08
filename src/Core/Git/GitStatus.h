//
// The change set of a working tree, as git sees it (docs/usability-round.md §0): what 'git status' lists, as plain
// data. Built on Process by composition; knows nothing about the workspace or views.
//
// The format is 'git status --porcelain=v1 -z': documented as stable across git versions and not changed by user
// configuration, NUL-separated so names come unquoted. The options that configuration could still change are given
// explicitly (untracked files), and '--no-optional-locks' keeps a background status from taking the index lock
// while an agent runs git in the terminal.
//

#ifndef GOATEDIT_GITSTATUS_H
#define GOATEDIT_GITSTATUS_H

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace gedit {
    class GitStatus {
    public:
        // Heaviest last: a path git gives two codes for (a staged add later deleted) takes the heavier, and a folder
        // holding several kinds can show the heaviest
        enum class kChange : uint8_t {
            kNone,
            kModified,      // 'M' / 'T', staged or not
            kAdded,         // 'A', staged
            kRenamed,       // 'R' / 'C' - 'path' is the new name, 'fromPath' the old
            kUntracked,     // '??' - a folder when everything below it is untracked ('u/')
            kDeleted,       // 'D', staged or not - not on disk
            kConflicted,    // unmerged: 'U' on either side, 'AA', 'DD'
        };

        struct Change {
            std::string path;           // relative to the repository's top level, '/' separated; a folder ends in '/'
            std::string fromPath;       // kRenamed only
            kChange change = kChange::kNone;
            char index = ' ';           // X and Y as git wrote them
            char workTree = ' ';
        };

        struct Snapshot {
            // false: not in a repository, or no git - 'error' says which. true with an 'error': the repository is
            // there but the status failed - keep what was shown before.
            bool isRepository = false;
            std::filesystem::path topLevel;         // what 'Change::path' is relative to
            std::filesystem::path gitDir;           // where 'index' and 'HEAD' are - not always '<top>/.git' (worktrees)
            std::vector<Change> changes;
            std::string error;
        };

        using DoneDelegate = std::function<void(const Snapshot &snapshot)>;
    public:
        // In the background: two git calls, one after the other, and 'onDone' on the last one's thread. Only what is
        // under 'root' is listed. The caller posts to the main thread.
        static void Query(const std::filesystem::path &root, DoneDelegate onDone);
        // The same, waiting for the answer - for a caller already off the main thread, and tests
        static Snapshot QueryAndWait(const std::filesystem::path &root);

        // The output of 'status --porcelain=v1 -z'. Plain text in, plain data out.
        static std::vector<Change> Parse(std::string_view porcelain);
        static kChange ChangeOf(char index, char workTree);
    };
}

#endif //GOATEDIT_GITSTATUS_H

//
// One project root's change set, made from a GitStatus snapshot (docs/usability-round.md §0): what changed, by the
// paths the workspace knows its nodes by. Immutable - a new snapshot makes a new one - so the main thread can hold it
// while the next is built on git's thread.
//
// Looked up when a row is drawn, not stored on the nodes: a folder expanded later, or a reconcile that rebuilds its
// children, would otherwise show its files unmarked until the next status.
//

#ifndef GOATEDIT_CHANGESET_H
#define GOATEDIT_CHANGESET_H

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "GitStatus.h"

namespace gedit {
    class ChangeSet {
    public:
        using Ref = std::shared_ptr<const ChangeSet>;
        using kChange = GitStatus::kChange;
    public:
        ChangeSet() = default;
        virtual ~ChangeSet() = default;

        // 'rootPath' as the workspace has it: the snapshot's paths are moved from git's top level onto it, so they
        // match the nodes' paths whatever symlinks lie between. What is outside the root is left out.
        static Ref Create(const GitStatus::Snapshot &snapshot, const std::filesystem::path &rootPath);

        // A file or folder git lists, or one inside a folder git lists as untracked. kNone: not in the change set.
        kChange ChangeOf(const std::filesystem::path &path) const;
        // The heaviest change at or below a folder - to show where the changes are in a collapsed tree
        kChange FolderChangeOf(const std::filesystem::path &folder) const;
        // Not on disk, so no node of the tree has them unless added
        const std::vector<std::filesystem::path> &Deleted() const {
            return deleted;
        }
        bool IsEmpty() const {
            return changes.empty();
        }
        // Same paths, same changes - nothing to redraw
        bool IsSameAs(const ChangeSet &other) const;

        // The form paths are compared in: lexically normal, no trailing separator, '.' for an empty path
        static std::string Key(const std::filesystem::path &path);
    private:
        void Add(const std::filesystem::path &path, kChange change, bool isFolder);
    private:
        std::string rootKey;
        std::unordered_map<std::string, kChange> changes;
        std::unordered_map<std::string, kChange> folders;           // every folder from a change up to the root
        std::unordered_set<std::string> untrackedFolders;
        std::vector<std::filesystem::path> deleted;
    };
}

#endif //GOATEDIT_CHANGESET_H

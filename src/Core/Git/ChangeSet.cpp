//
// ChangeSet - a GitStatus snapshot by the workspace's own paths
//

#include <system_error>

#include "ChangeSet.h"

using namespace gedit;
namespace fs = std::filesystem;

ChangeSet::Ref ChangeSet::Create(const GitStatus::Snapshot &snapshot, const fs::path &rootPath) {
    auto changeSet = std::make_shared<ChangeSet>();
    changeSet->rootKey = Key(rootPath);
    if (!snapshot.isRepository || snapshot.topLevel.empty()) {
        return changeSet;
    }

    // Where the root sits below the top level - by real paths, as git gives its top level
    std::error_code error;
    auto realRoot = fs::weakly_canonical(rootPath, error);
    auto realTop = fs::weakly_canonical(snapshot.topLevel, error);
    auto rootFromTop = Key(realRoot.lexically_relative(realTop));
    if (rootFromTop.empty() || (rootFromTop == "..") || (rootFromTop.rfind("../", 0) == 0)) {
        return changeSet;
    }
    std::string prefix = (rootFromTop == ".") ? "" : rootFromTop + "/";

    for (auto &change : snapshot.changes) {
        if (change.path.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }
        auto relative = change.path.substr(prefix.size());
        bool isFolder = !relative.empty() && (relative.back() == '/');
        auto path = rootPath / relative;
        changeSet->Add(path, change.change, isFolder);
        if (change.change == kChange::kDeleted) {
            changeSet->deleted.push_back(fs::path(Key(path)));
        }
    }
    return changeSet;
}

void ChangeSet::Add(const fs::path &path, kChange change, bool isFolder) {
    auto key = Key(path);
    auto &current = changes[key];
    if (change > current) {
        current = change;
    }
    if (isFolder && (change == kChange::kUntracked)) {
        untrackedFolders.insert(key);
    }
    // The folders up to the root, the root included - a folder listed itself counts for itself
    auto folder = isFolder ? fs::path(key) : fs::path(key).parent_path();
    while (true) {
        auto folderKey = Key(folder);
        auto &heaviest = folders[folderKey];
        if (change > heaviest) {
            heaviest = change;
        }
        if ((folderKey == rootKey) || folder.empty()) {
            break;
        }
        auto parent = folder.parent_path();
        if (parent == folder) {
            break;
        }
        folder = parent;
    }
}

ChangeSet::kChange ChangeSet::ChangeOf(const fs::path &path) const {
    auto key = Key(path);
    if (auto it = changes.find(key); it != changes.end()) {
        return it->second;
    }
    if (untrackedFolders.empty()) {
        return kChange::kNone;
    }
    // Inside an untracked folder: git lists the folder, not what is in it
    auto folder = fs::path(key).parent_path();
    while (true) {
        auto folderKey = Key(folder);
        if (folderKey.size() < rootKey.size()) {
            break;
        }
        if (untrackedFolders.count(folderKey) != 0) {
            return kChange::kUntracked;
        }
        auto parent = folder.parent_path();
        if (parent == folder) {
            break;
        }
        folder = parent;
    }
    return kChange::kNone;
}

ChangeSet::kChange ChangeSet::FolderChangeOf(const fs::path &folder) const {
    if (auto it = folders.find(Key(folder)); it != folders.end()) {
        return it->second;
    }
    // A folder inside an untracked one holds nothing but untracked files
    return ChangeOf(folder);
}

bool ChangeSet::IsSameAs(const ChangeSet &other) const {
    return (rootKey == other.rootKey) && (changes == other.changes);
}

std::string ChangeSet::Key(const fs::path &path) {
    auto key = path.lexically_normal().string();
    while ((key.size() > 1) && (key.back() == '/')) {
        key.pop_back();
    }
    // The parent of a top-level name in a root opened as '.'
    if (key.empty()) {
        key = ".";
    }
    return key;
}

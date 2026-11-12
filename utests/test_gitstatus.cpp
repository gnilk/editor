//
// GitStatus (docs/usability-round.md §0): the porcelain parser on fixed output, then real git on temp repositories.
//
#include <testinterface.h>

#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>
#include <filesystem>
#include <fstream>
#include <string>

#include "Core/Git/ChangeSet.h"
#include "Core/Git/GitStatus.h"
#include "Core/Git/RepoMonitor.h"
#include "Core/Config/Config.h"
#include "Core/Workspace.h"
#include "Core/Editor/Views/HSplitViewStatus.h"

using namespace gedit;
namespace fs = std::filesystem;
using kChange = GitStatus::kChange;

extern "C" {
    DLL_EXPORT int test_gitstatus(ITesting *t);
    DLL_EXPORT int test_gitstatus_parse(ITesting *t);
    DLL_EXPORT int test_gitstatus_parse_rename(ITesting *t);
    DLL_EXPORT int test_gitstatus_parse_names(ITesting *t);
    DLL_EXPORT int test_gitstatus_parse_broken(ITesting *t);
    DLL_EXPORT int test_gitstatus_changeof(ITesting *t);
    DLL_EXPORT int test_gitstatus_repository(ITesting *t);
    DLL_EXPORT int test_gitstatus_subfolder(ITesting *t);
    DLL_EXPORT int test_gitstatus_norepository(ITesting *t);
    DLL_EXPORT int test_gitstatus_clean(ITesting *t);
    DLL_EXPORT int test_gitstatus_changeset(ITesting *t);
    DLL_EXPORT int test_gitstatus_changeset_untracked(ITesting *t);
    DLL_EXPORT int test_gitstatus_changeset_subfolder(ITesting *t);
    DLL_EXPORT int test_gitstatus_changeset_same(ITesting *t);
    DLL_EXPORT int test_gitstatus_monitor(ITesting *t);
    DLL_EXPORT int test_gitstatus_monitor_norepository(ITesting *t);
    DLL_EXPORT int test_gitstatus_workspace(ITesting *t);
    DLL_EXPORT int test_gitstatus_parse_branch(ITesting *t);
    DLL_EXPORT int test_gitstatus_branch(ITesting *t);
    DLL_EXPORT int test_gitstatus_changeset_branch(ITesting *t);
    DLL_EXPORT int test_gitstatus_monitor_branch(ITesting *t);
    DLL_EXPORT int test_gitstatus_workspace_branch(ITesting *t);
    DLL_EXPORT int test_gitstatus_branchlabel(ITesting *t);
}

namespace {
    std::string Z(std::initializer_list<std::string> fields) {
        std::string text;
        for (auto &field : fields) {
            text += field;
            text.push_back('\0');
        }
        return text;
    }

    fs::path MakeRoot(const std::string &name) {
        auto root = fs::canonical(fs::temp_directory_path()) / ("goatedit_gitstatus_" + name);
        fs::remove_all(root);
        fs::create_directories(root);
        return root;
    }

    void WriteFile(const fs::path &path, const std::string &text) {
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file << text;
    }

    bool Git(const fs::path &root, const std::string &arguments) {
        auto command = "git -C '" + root.string() + "' -c user.name=test -c user.email=test@test -c commit.gpgsign=false "
                       + arguments + " >/dev/null 2>&1";
        return std::system(command.c_str()) == 0;
    }

    GitStatus::Snapshot MakeSnapshot(const fs::path &topLevel, std::initializer_list<std::string> fields) {
        GitStatus::Snapshot snapshot;
        snapshot.isRepository = true;
        snapshot.topLevel = topLevel;
        snapshot.changes = GitStatus::Parse(Z(fields));
        return snapshot;
    }

    // Collects what a RepoMonitor hands over; 'postToMain' runs it at once, on git's thread
    struct Received {
        std::mutex lock;
        ChangeSet::Ref changeSet = nullptr;
        int count = 0;

        RepoMonitor::Ref Monitor(const fs::path &root) {
            auto monitor = RepoMonitor::Create(root, [](std::function<void()> callback) {
                callback();
            }, std::chrono::milliseconds(20));
            monitor->SetChangedHandler([this](const ChangeSet::Ref &newChangeSet) {
                std::lock_guard<std::mutex> guard(lock);
                changeSet = newChangeSet;
                count++;
            });
            return monitor;
        }

        // Until 'isDone' holds for what was received, or 5 seconds
        template<typename Predicate>
        bool WaitFor(Predicate isDone) {
            for (int i = 0; i < 250; i++) {
                {
                    std::lock_guard<std::mutex> guard(lock);
                    if ((changeSet != nullptr) && isDone(*changeSet)) {
                        return true;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            return false;
        }
    };

    // The main thread's queue, run by the test: what the monitor posts runs here, as it would in the editor
    struct MainQueue {
        std::mutex lock;
        std::vector<std::function<void()>> pending;

        RepoMonitor::PostDelegate Post() {
            return [this](std::function<void()> callback) {
                std::lock_guard<std::mutex> guard(lock);
                pending.push_back(std::move(callback));
            };
        }

        // Runs what was posted until 'isDone', or 5 seconds
        template<typename Predicate>
        bool PumpUntil(Predicate isDone) {
            for (int i = 0; i < 250; i++) {
                std::vector<std::function<void()>> callbacks;
                {
                    std::lock_guard<std::mutex> guard(lock);
                    callbacks.swap(pending);
                }
                for (auto &callback : callbacks) {
                    callback();
                }
                if (isDone()) {
                    return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            return false;
        }
    };

    const GitStatus::Change *Find(const GitStatus::Snapshot &snapshot, const std::string &path) {
        for (auto &change : snapshot.changes) {
            if (change.path == path) {
                return &change;
            }
        }
        return nullptr;
    }
}

DLL_EXPORT int test_gitstatus(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_parse(ITesting *t) {
    auto changes = GitStatus::Parse(Z({" M src/a.cpp", "M  src/b.cpp", "A  new.h", " D gone.txt", "?? u/", "?? n.txt",
                                       "UU both.cpp", "!! build/"}));
    TR_ASSERT(t, changes.size() == 7);      // ignored ('!!') isn't a change
    TR_ASSERT(t, (changes[0].path == "src/a.cpp") && (changes[0].change == kChange::kModified));
    TR_ASSERT(t, (changes[0].index == ' ') && (changes[0].workTree == 'M'));
    TR_ASSERT(t, (changes[1].path == "src/b.cpp") && (changes[1].change == kChange::kModified));
    TR_ASSERT(t, changes[2].change == kChange::kAdded);
    TR_ASSERT(t, changes[3].change == kChange::kDeleted);
    TR_ASSERT(t, (changes[4].path == "u/") && (changes[4].change == kChange::kUntracked));
    TR_ASSERT(t, changes[5].change == kChange::kUntracked);
    TR_ASSERT(t, changes[6].change == kChange::kConflicted);
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_parse_rename(ITesting *t) {
    // The old name is a field of its own, after the new one - the entry after it must not be lost
    auto changes = GitStatus::Parse(Z({"R  moved.txt", "a.txt", " M after.txt"}));
    TR_ASSERT(t, changes.size() == 2);
    TR_ASSERT(t, changes[0].change == kChange::kRenamed);
    TR_ASSERT(t, changes[0].path == "moved.txt");
    TR_ASSERT(t, changes[0].fromPath == "a.txt");
    TR_ASSERT(t, changes[1].path == "after.txt");
    TR_ASSERT(t, changes[1].fromPath.empty());
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_parse_names(ITesting *t) {
    // -z: no quoting, so spaces, quotes and non-ASCII come as they are
    auto changes = GitStatus::Parse(Z({" M b c.txt", "?? \"quoted\".txt", "?? r\xC3\xA4ksm\xC3\xB6rg\xC3\xA5s.md"}));
    TR_ASSERT(t, changes.size() == 3);
    TR_ASSERT(t, changes[0].path == "b c.txt");
    TR_ASSERT(t, changes[1].path == "\"quoted\".txt");
    TR_ASSERT(t, changes[2].path == "r\xC3\xA4ksm\xC3\xB6rg\xC3\xA5s.md");
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_parse_broken(ITesting *t) {
    TR_ASSERT(t, GitStatus::Parse("").empty());
    TR_ASSERT(t, GitStatus::Parse(Z({"", "M"})).empty());
    // No NUL at the end, and a rename cut off before its old name
    auto changes = GitStatus::Parse(" M last.txt");
    TR_ASSERT(t, (changes.size() == 1) && (changes[0].path == "last.txt"));
    changes = GitStatus::Parse(std::string("R  new.txt\0", 11));
    TR_ASSERT(t, (changes.size() == 1) && changes[0].fromPath.empty());
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_changeof(ITesting *t) {
    TR_ASSERT(t, GitStatus::ChangeOf(' ', 'M') == kChange::kModified);
    TR_ASSERT(t, GitStatus::ChangeOf('M', 'M') == kChange::kModified);
    TR_ASSERT(t, GitStatus::ChangeOf(' ', 'T') == kChange::kModified);
    TR_ASSERT(t, GitStatus::ChangeOf('A', ' ') == kChange::kAdded);
    TR_ASSERT(t, GitStatus::ChangeOf('A', 'M') == kChange::kAdded);
    TR_ASSERT(t, GitStatus::ChangeOf(' ', 'A') == kChange::kAdded);      // intent to add
    TR_ASSERT(t, GitStatus::ChangeOf('C', ' ') == kChange::kAdded);
    TR_ASSERT(t, GitStatus::ChangeOf('R', 'M') == kChange::kRenamed);
    TR_ASSERT(t, GitStatus::ChangeOf('A', 'D') == kChange::kDeleted);     // added, then gone from disk
    TR_ASSERT(t, GitStatus::ChangeOf('D', ' ') == kChange::kDeleted);
    TR_ASSERT(t, GitStatus::ChangeOf('U', 'U') == kChange::kConflicted);
    TR_ASSERT(t, GitStatus::ChangeOf('A', 'A') == kChange::kConflicted);
    TR_ASSERT(t, GitStatus::ChangeOf('D', 'D') == kChange::kConflicted);
    TR_ASSERT(t, GitStatus::ChangeOf('D', 'U') == kChange::kConflicted);
    TR_ASSERT(t, GitStatus::ChangeOf('?', '?') == kChange::kUntracked);
    TR_ASSERT(t, GitStatus::ChangeOf('!', '!') == kChange::kNone);
    TR_ASSERT(t, GitStatus::ChangeOf(' ', ' ') == kChange::kNone);
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_repository(ITesting *t) {
    auto root = MakeRoot("repository");
    TR_ASSERT(t, Git(root, "init -q"));
    WriteFile(root / "a.txt", "a\n");
    WriteFile(root / "b c.txt", "b\n");
    WriteFile(root / "sub/x.txt", "x\n");
    TR_ASSERT(t, Git(root, "add ."));
    TR_ASSERT(t, Git(root, "commit -q -m initial"));

    TR_ASSERT(t, Git(root, "mv a.txt moved.txt"));
    WriteFile(root / "b c.txt", "b\nchanged\n");
    WriteFile(root / "new.txt", "n\n");
    WriteFile(root / "staged.txt", "s\n");
    TR_ASSERT(t, Git(root, "add staged.txt"));
    fs::remove(root / "sub/x.txt");
    WriteFile(root / "untracked/f.txt", "f\n");

    auto snapshot = GitStatus::QueryAndWait(root);
    TR_ASSERT(t, snapshot.isRepository);
    TR_ASSERT(t, snapshot.error.empty());
    TR_ASSERT(t, fs::canonical(snapshot.topLevel) == root);
    TR_ASSERT(t, fs::canonical(snapshot.gitDir) == root / ".git");
    TR_ASSERT(t, snapshot.changes.size() == 6);

    auto moved = Find(snapshot, "moved.txt");
    TR_ASSERT(t, (moved != nullptr) && (moved->change == kChange::kRenamed) && (moved->fromPath == "a.txt"));
    auto modified = Find(snapshot, "b c.txt");
    TR_ASSERT(t, (modified != nullptr) && (modified->change == kChange::kModified));
    auto untracked = Find(snapshot, "new.txt");
    TR_ASSERT(t, (untracked != nullptr) && (untracked->change == kChange::kUntracked));
    auto added = Find(snapshot, "staged.txt");
    TR_ASSERT(t, (added != nullptr) && (added->change == kChange::kAdded));
    auto deleted = Find(snapshot, "sub/x.txt");
    TR_ASSERT(t, (deleted != nullptr) && (deleted->change == kChange::kDeleted));
    // A folder of untracked files is one entry, the folder
    auto folder = Find(snapshot, "untracked/");
    TR_ASSERT(t, (folder != nullptr) && (folder->change == kChange::kUntracked));
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_subfolder(ITesting *t) {
    // A workspace below the top level: only what is under it is listed, with paths from the top level
    auto top = MakeRoot("subfolder");
    TR_ASSERT(t, Git(top, "init -q"));
    WriteFile(top / "outside.txt", "o\n");
    WriteFile(top / "project/inside.txt", "i\n");
    TR_ASSERT(t, Git(top, "add ."));
    TR_ASSERT(t, Git(top, "commit -q -m initial"));
    WriteFile(top / "outside.txt", "o\nchanged\n");
    WriteFile(top / "project/inside.txt", "i\nchanged\n");

    auto snapshot = GitStatus::QueryAndWait(top / "project");
    TR_ASSERT(t, snapshot.isRepository);
    TR_ASSERT(t, fs::canonical(snapshot.topLevel) == top);
    TR_ASSERT(t, snapshot.changes.size() == 1);
    TR_ASSERT(t, snapshot.changes[0].path == "project/inside.txt");
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_norepository(ITesting *t) {
    auto root = MakeRoot("norepository");
    // The temp folder must not be inside a repository itself, or this tests nothing
    TR_ASSERT(t, !Git(root, "rev-parse --show-toplevel"));
    auto snapshot = GitStatus::QueryAndWait(root);
    TR_ASSERT(t, !snapshot.isRepository);
    TR_ASSERT(t, !snapshot.error.empty());
    TR_ASSERT(t, snapshot.changes.empty());
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_clean(ITesting *t) {
    auto root = MakeRoot("clean");
    TR_ASSERT(t, Git(root, "init -q"));
    WriteFile(root / "a.txt", "a\n");
    TR_ASSERT(t, Git(root, "add ."));
    TR_ASSERT(t, Git(root, "commit -q -m initial"));
    auto snapshot = GitStatus::QueryAndWait(root);
    TR_ASSERT(t, snapshot.isRepository);
    TR_ASSERT(t, snapshot.error.empty());
    TR_ASSERT(t, snapshot.changes.empty());
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_changeset(ITesting *t) {
    auto root = MakeRoot("changeset");
    auto snapshot = MakeSnapshot(root, {" M src/core/a.cpp", "A  src/b.cpp", " D gone.txt", "UU src/core/c.cpp"});
    auto changeSet = ChangeSet::Create(snapshot, root);
    TR_ASSERT(t, changeSet->ChangeOf(root / "src/core/a.cpp") == kChange::kModified);
    TR_ASSERT(t, changeSet->ChangeOf(root / "src/b.cpp") == kChange::kAdded);
    TR_ASSERT(t, changeSet->ChangeOf(root / "src/other.cpp") == kChange::kNone);
    // A path written another way is the same path
    TR_ASSERT(t, changeSet->ChangeOf(root / "src/./core/../b.cpp") == kChange::kAdded);
    // Folders: the heaviest at or below, up to the root
    TR_ASSERT(t, changeSet->FolderChangeOf(root / "src/core") == kChange::kConflicted);
    TR_ASSERT(t, changeSet->FolderChangeOf(root / "src") == kChange::kConflicted);
    TR_ASSERT(t, changeSet->FolderChangeOf(root) == kChange::kConflicted);
    TR_ASSERT(t, changeSet->FolderChangeOf(root / "docs") == kChange::kNone);
    TR_ASSERT(t, changeSet->Deleted().size() == 1);
    TR_ASSERT(t, changeSet->Deleted()[0] == root / "gone.txt");
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_changeset_untracked(ITesting *t) {
    // Git lists an untracked folder, not its files: what is in it, at any depth, is untracked
    auto root = MakeRoot("changeset_untracked");
    auto changeSet = ChangeSet::Create(MakeSnapshot(root, {"?? new/"}), root);
    TR_ASSERT(t, changeSet->ChangeOf(root / "new") == kChange::kUntracked);
    TR_ASSERT(t, changeSet->ChangeOf(root / "new/a.txt") == kChange::kUntracked);
    TR_ASSERT(t, changeSet->ChangeOf(root / "new/deep/b.txt") == kChange::kUntracked);
    TR_ASSERT(t, changeSet->FolderChangeOf(root / "new/deep") == kChange::kUntracked);
    TR_ASSERT(t, changeSet->ChangeOf(root / "newer.txt") == kChange::kNone);
    TR_ASSERT(t, changeSet->FolderChangeOf(root) == kChange::kUntracked);
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_changeset_subfolder(ITesting *t) {
    // The root below git's top level, and named through a symlink: the nodes' paths are what is matched
    auto top = MakeRoot("changeset_subfolder");
    fs::create_directories(top / "project");
    auto link = fs::canonical(fs::temp_directory_path()) / "goatedit_gitstatus_link";
    fs::remove(link);
    fs::create_directory_symlink(top / "project", link);
    auto changeSet = ChangeSet::Create(MakeSnapshot(top, {" M project/a.txt", " M other/b.txt"}), link);
    TR_ASSERT(t, changeSet->ChangeOf(link / "a.txt") == kChange::kModified);
    TR_ASSERT(t, changeSet->ChangeOf(link / "other/b.txt") == kChange::kNone);
    TR_ASSERT(t, changeSet->FolderChangeOf(link) == kChange::kModified);
    // Not in a repository: nothing
    GitStatus::Snapshot none;
    TR_ASSERT(t, ChangeSet::Create(none, link)->IsEmpty());
    fs::remove(link);
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_changeset_same(ITesting *t) {
    auto root = MakeRoot("changeset_same");
    auto a = ChangeSet::Create(MakeSnapshot(root, {" M a.txt", "?? b.txt"}), root);
    auto b = ChangeSet::Create(MakeSnapshot(root, {"?? b.txt", " M a.txt"}), root);
    auto c = ChangeSet::Create(MakeSnapshot(root, {"M  a.txt", "?? b.txt"}), root);       // staged now - same change
    auto d = ChangeSet::Create(MakeSnapshot(root, {"A  a.txt", "?? b.txt"}), root);
    TR_ASSERT(t, a->IsSameAs(*b));
    TR_ASSERT(t, a->IsSameAs(*c));
    TR_ASSERT(t, !a->IsSameAs(*d));
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_monitor(ITesting *t) {
    auto root = MakeRoot("monitor");
    TR_ASSERT(t, Git(root, "init -q"));
    WriteFile(root / "a.txt", "a\n");
    TR_ASSERT(t, Git(root, "add ."));
    TR_ASSERT(t, Git(root, "commit -q -m initial"));

    Received received;
    auto monitor = received.Monitor(root);
    monitor->Start();
    TR_ASSERT(t, received.WaitFor([](const ChangeSet &changeSet) {
        return changeSet.IsEmpty();
    }));

    // A file written: the folder monitor would ask
    WriteFile(root / "a.txt", "a\nchanged\n");
    monitor->Refresh();
    TR_ASSERT(t, received.WaitFor([&root](const ChangeSet &changeSet) {
        return changeSet.ChangeOf(root / "a.txt") == kChange::kModified;
    }));

    // A commit touches no file the folder monitor sees - the git directory check finds it
    TR_ASSERT(t, Git(root, "commit -q -a -m second"));
    TR_ASSERT(t, received.WaitFor([](const ChangeSet &changeSet) {
        return changeSet.IsEmpty();
    }));

    // Asked again with nothing new: no new change set is handed over
    int countBefore = 0;
    {
        std::lock_guard<std::mutex> guard(received.lock);
        countBefore = received.count;
    }
    monitor->Refresh();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    {
        std::lock_guard<std::mutex> guard(received.lock);
        TR_ASSERT(t, received.count == countBefore);
    }
    monitor->Stop();
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_monitor_norepository(ITesting *t) {
    // Not a repository: one empty change set, and the check finds nothing to look at
    auto root = MakeRoot("monitor_norepository");
    Received received;
    auto monitor = received.Monitor(root);
    monitor->Start();
    TR_ASSERT(t, received.WaitFor([](const ChangeSet &changeSet) {
        return changeSet.IsEmpty();
    }));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    {
        std::lock_guard<std::mutex> guard(received.lock);
        TR_ASSERT(t, received.count == 1);
    }
    monitor->Stop();
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_workspace(ITesting *t) {
    auto root = MakeRoot("workspace");
    TR_ASSERT(t, Git(root, "init -q"));
    WriteFile(root / "a.txt", "a\n");
    WriteFile(root / "sub/b.txt", "b\n");
    WriteFile(root / "sub/c.txt", "c\n");
    TR_ASSERT(t, Git(root, "add ."));
    TR_ASSERT(t, Git(root, "commit -q -m initial"));
    WriteFile(root / "a.txt", "a\nchanged\n");
    fs::remove(root / "sub/b.txt");
    WriteFile(root / "new.txt", "n\n");

    auto cfgWorkspace = Config::Instance()["workspace"];
    auto oldDepth = cfgWorkspace.GetInt("max_scan_depth", 1);
    cfgWorkspace.SetInt("max_scan_depth", 4);

    MainQueue mainQueue;
    Workspace workspace;
    workspace.SetRepoMonitor([&mainQueue](const fs::path &rootPath) {
        return RepoMonitor::Create(rootPath, mainQueue.Post(), std::chrono::milliseconds(20));
    });
    TR_ASSERT(t, workspace.OpenFolder(root.string()));
    cfgWorkspace.SetInt("max_scan_depth", oldDepth);

    auto rootNode = workspace.GetProjectRoots().back()->GetRootNode();
    auto nodeA = rootNode->FindNodeWithPath(root / "a.txt");
    auto nodeNew = rootNode->FindNodeWithPath(root / "new.txt");
    auto nodeSub = rootNode->FindNodeWithPath(root / "sub");
    auto nodeC = rootNode->FindNodeWithPath(root / "sub/c.txt");
    TR_ASSERT(t, (nodeA != nullptr) && (nodeNew != nullptr) && (nodeSub != nullptr) && (nodeC != nullptr));
    // Not read from disk - the scan found no such file
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "sub/b.txt") == nullptr);

    TR_ASSERT(t, mainQueue.PumpUntil([&]() {
        return workspace.ChangeOf(nodeA) == kChange::kModified;
    }));
    TR_ASSERT(t, workspace.ChangeOf(nodeNew) == kChange::kUntracked);
    TR_ASSERT(t, workspace.ChangeOf(nodeC) == kChange::kNone);
    TR_ASSERT(t, workspace.ChangeOf(nodeSub) == kChange::kDeleted);     // the heaviest below it
    TR_ASSERT(t, workspace.ChangeOf(rootNode) == kChange::kDeleted);

    // The deleted file is in the tree, struck through
    auto nodeB = rootNode->FindNodeWithPath(root / "sub/b.txt");
    TR_ASSERT(t, nodeB != nullptr);
    TR_ASSERT(t, nodeB->IsMissingOnDisk());
    TR_ASSERT(t, nodeB->IsDeletedInChangeSet());
    TR_ASSERT(t, workspace.ChangeOf(nodeB) == kChange::kDeleted);
    // A reconcile of its folder (the folder monitor's) keeps it
    workspace.ReconcileFolder(nodeSub);
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "sub/b.txt") == nodeB);

    // Committed: the change set is empty, and the deleted file leaves the tree - no folder monitor, the git
    // directory check sees the commit
    TR_ASSERT(t, Git(root, "add -A"));
    TR_ASSERT(t, Git(root, "commit -q -m second"));
    TR_ASSERT(t, mainQueue.PumpUntil([&]() {
        return rootNode->FindNodeWithPath(root / "sub/b.txt") == nullptr;
    }));
    TR_ASSERT(t, workspace.ChangeOf(nodeA) == kChange::kNone);
    TR_ASSERT(t, workspace.ChangeOf(nodeSub) == kChange::kNone);
    TR_ASSERT(t, workspace.ChangeOf(rootNode) == kChange::kNone);
    return kTR_Pass;
}

//
// The branch (docs/usability-round.md §9)
//
DLL_EXPORT int test_gitstatus_parse_branch(ITesting *t) {
    auto branch = GitStatus::ParseBranch(Z({"## main...origin/main [ahead 1, behind 2]", " M a.txt"}));
    TR_ASSERT(t, (branch.name == "main") && !branch.isDetached);
    branch = GitStatus::ParseBranch(Z({"## feature/x-y"}));
    TR_ASSERT(t, (branch.name == "feature/x-y") && !branch.isDetached);
    // An upstream that is gone
    branch = GitStatus::ParseBranch(Z({"## topic...origin/topic [gone]"}));
    TR_ASSERT(t, branch.name == "topic");
    branch = GitStatus::ParseBranch(Z({"## No commits yet on trunk"}));
    TR_ASSERT(t, branch.name == "trunk");
    branch = GitStatus::ParseBranch(Z({"## Initial commit on trunk"}));
    TR_ASSERT(t, branch.name == "trunk");
    // Detached: git doesn't name the commit here
    branch = GitStatus::ParseBranch(Z({"## HEAD (no branch)"}));
    TR_ASSERT(t, branch.isDetached && branch.name.empty());
    // No '--branch' record: nothing
    TR_ASSERT(t, GitStatus::ParseBranch(Z({" M a.txt"})).name.empty());
    TR_ASSERT(t, GitStatus::ParseBranch("").name.empty());
    // ...and the record isn't a change
    TR_ASSERT(t, GitStatus::Parse(Z({"## main...origin/main", " M a.txt"})).size() == 1);
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_branch(ITesting *t) {
    auto root = MakeRoot("branch");
    TR_ASSERT(t, Git(root, "init -q -b trunk"));
    // No commit yet: the branch is still named
    auto snapshot = GitStatus::QueryAndWait(root);
    TR_ASSERT(t, snapshot.branch.name == "trunk");
    TR_ASSERT(t, !snapshot.branch.isDetached);

    WriteFile(root / "a.txt", "a\n");
    TR_ASSERT(t, Git(root, "add ."));
    TR_ASSERT(t, Git(root, "commit -q -m initial"));
    TR_ASSERT(t, Git(root, "switch -q -c feature/thing"));
    snapshot = GitStatus::QueryAndWait(root);
    TR_ASSERT(t, snapshot.branch.name == "feature/thing");

    // Detached: the short SHA stands for it
    TR_ASSERT(t, Git(root, "switch -q --detach"));
    snapshot = GitStatus::QueryAndWait(root);
    TR_ASSERT(t, snapshot.branch.isDetached);
    TR_ASSERT(t, snapshot.branch.name.size() >= 4);
    TR_ASSERT(t, snapshot.branch.name.find_first_not_of("0123456789abcdef") == std::string::npos);
    TR_ASSERT(t, snapshot.changes.empty());

    // Not a repository: none
    auto plain = MakeRoot("branch_plain");
    snapshot = GitStatus::QueryAndWait(plain);
    TR_ASSERT(t, snapshot.branch.name.empty());
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_changeset_branch(ITesting *t) {
    auto root = MakeRoot("changeset_branch");
    auto snapshotMain = MakeSnapshot(root, {" M a.txt"});
    snapshotMain.branch.name = "main";
    auto snapshotTopic = snapshotMain;
    snapshotTopic.branch.name = "topic";
    auto main = ChangeSet::Create(snapshotMain, root);
    auto topic = ChangeSet::Create(snapshotTopic, root);
    TR_ASSERT(t, main->GetBranch().name == "main");
    // Same working tree, another branch: not the same - the monitor must hand it over
    TR_ASSERT(t, !main->IsSameAs(*topic));
    TR_ASSERT(t, main->IsSameAs(*ChangeSet::Create(snapshotMain, root)));
    // Not a repository: no branch
    GitStatus::Snapshot none;
    none.branch.name = "stale";
    TR_ASSERT(t, ChangeSet::Create(none, root)->GetBranch().name.empty());
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_monitor_branch(ITesting *t) {
    // 'git switch' in the terminal between two branches on the same commit: no file changes, only the branch
    auto root = MakeRoot("monitor_branch");
    TR_ASSERT(t, Git(root, "init -q -b main"));
    WriteFile(root / "a.txt", "a\n");
    TR_ASSERT(t, Git(root, "add ."));
    TR_ASSERT(t, Git(root, "commit -q -m initial"));
    TR_ASSERT(t, Git(root, "branch topic"));

    Received received;
    auto monitor = received.Monitor(root);
    monitor->Start();
    TR_ASSERT(t, received.WaitFor([](const ChangeSet &changeSet) {
        return changeSet.GetBranch().name == "main";
    }));
    // The git directory check sees HEAD change
    TR_ASSERT(t, Git(root, "switch -q topic"));
    TR_ASSERT(t, received.WaitFor([](const ChangeSet &changeSet) {
        return changeSet.GetBranch().name == "topic";
    }));
    monitor->Stop();
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_workspace_branch(ITesting *t) {
    auto root = MakeRoot("workspace_branch");
    TR_ASSERT(t, Git(root, "init -q -b main"));
    WriteFile(root / "a.txt", "a\n");
    TR_ASSERT(t, Git(root, "add ."));
    TR_ASSERT(t, Git(root, "commit -q -m initial"));

    MainQueue mainQueue;
    Workspace workspace;
    workspace.SetRepoMonitor([&mainQueue](const fs::path &rootPath) {
        return RepoMonitor::Create(rootPath, mainQueue.Post(), std::chrono::milliseconds(20));
    });
    // Nothing opened yet: no branch
    TR_ASSERT(t, workspace.GetOpenedFolderBranch().name.empty());
    TR_ASSERT(t, workspace.OpenFolder(root.string()));
    TR_ASSERT(t, mainQueue.PumpUntil([&]() {
        return workspace.GetOpenedFolderBranch().name == "main";
    }));
    TR_ASSERT(t, Git(root, "switch -q -c other"));
    TR_ASSERT(t, mainQueue.PumpUntil([&]() {
        return workspace.GetOpenedFolderBranch().name == "other";
    }));
    return kTR_Pass;
}

DLL_EXPORT int test_gitstatus_branchlabel(ITesting *t) {
    TR_ASSERT(t, HSplitViewStatus::BranchLabel({}).empty());
    TR_ASSERT(t, HSplitViewStatus::BranchLabel({"main", false}) == U"⎇ main");
    TR_ASSERT(t, HSplitViewStatus::BranchLabel({"a1b2c3d", true}) == U"⎇ (a1b2c3d)");
    // Cut in the middle: both ends stay, and it is as long as asked
    auto label = HSplitViewStatus::BranchLabel({"feature/very-long-branch-name-thing", false}, 15);
    TR_ASSERT(t, label == U"⎇ feature…e-thing");
    TR_ASSERT(t, label.size() == 2 + 15);
    // Exactly the limit: untouched
    TR_ASSERT(t, HSplitViewStatus::BranchLabel({"abcde", false}, 5) == U"⎇ abcde");
    return kTR_Pass;
}

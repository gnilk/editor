//
// Created by gnilk on 17.06.26.
//
// FolderScanner — the pure filesystem walk extracted out of Workspace (docs/folder-scanner.md, FS-1).
// These cases run over a throwaway temp tree with NO Workspace/Editor/singleton in scope: lambdas
// record each enter/file/leave event into a vector<FsEvent> and assert on the stream. Order between
// SIBLINGS is unspecified (std::filesystem::directory_iterator), so we assert STRUCTURE — event counts,
// reported depth, and that a directory's children land between its enter and leave — never a fixed
// sibling sequence. Depth-bound / symlink-cycle (FS-4) and exclude-prune (FS-2) cases are added here as
// those items land.
//
#include <testinterface.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Core/FileSystem/FolderScanner.h"

using namespace gedit;
namespace fs = std::filesystem;

extern "C" {
DLL_EXPORT int test_folderscanner(ITesting *t);
DLL_EXPORT int test_folderscanner_emptydir(ITesting *t);
DLL_EXPORT int test_folderscanner_flat(ITesting *t);
DLL_EXPORT int test_folderscanner_nested(ITesting *t);
DLL_EXPORT int test_folderscanner_veto(ITesting *t);
DLL_EXPORT int test_folderscanner_maxdepth(ITesting *t);
DLL_EXPORT int test_folderscanner_symlink_no_follow(ITesting *t);
DLL_EXPORT int test_folderscanner_symlink_follow(ITesting *t);
}

namespace {
    struct FsEvent {
        enum class Kind { EnterDir, File, LeaveDir };
        Kind kind;
        std::string name;   // filename only — stable regardless of the temp root
        int depth;
    };

    // RAII throwaway directory under the system temp dir. Built fresh (remove_all first) so a stale
    // dir from a prior run can't leak in; torn down in the destructor.
    struct TempTree {
        fs::path root;

        explicit TempTree(const std::string &tag) {
            static int counter = 0;
            root = fs::temp_directory_path() / ("goat_fs_" + tag + "_" + std::to_string(counter++));
            std::error_code ec;
            fs::remove_all(root, ec);
            fs::create_directories(root);
        }
        ~TempTree() {
            std::error_code ec;
            fs::remove_all(root, ec);
        }

        void Dir(const std::string &rel) {
            fs::create_directories(root / rel);
        }
        void File(const std::string &rel) {
            auto p = root / rel;
            fs::create_directories(p.parent_path());
            std::ofstream(p) << "x";
        }
        // Symlink at `link` -> directory `target` (both relative to root). Returns false if the
        // platform/filesystem refused, so the caller can skip rather than fail spuriously.
        bool Symlink(const std::string &target, const std::string &link) {
            std::error_code ec;
            fs::create_directory_symlink(root / target, root / link, ec);
            return !ec;
        }
    };

    // Drive the scanner over root with default options, recording every event.
    std::vector<FsEvent> Walk(const fs::path &root,
                              const FolderScanner::Options &opts = {},
                              bool descend = true) {
        std::vector<FsEvent> events;
        FolderScanner scanner;
        scanner.onEnterDir = [&](const fs::path &p, int depth) {
            events.push_back({FsEvent::Kind::EnterDir, p.filename().string(), depth});
            return descend;
        };
        scanner.onFile = [&](const fs::path &p, int depth) {
            events.push_back({FsEvent::Kind::File, p.filename().string(), depth});
        };
        scanner.onLeaveDir = [&](const fs::path &p, int depth) {
            events.push_back({FsEvent::Kind::LeaveDir, p.filename().string(), depth});
        };
        scanner.Scan(root, opts);
        return events;
    }

    int Count(const std::vector<FsEvent> &events, FsEvent::Kind kind) {
        int n = 0;
        for (const auto &e : events) {
            if (e.kind == kind) {
                n++;
            }
        }
        return n;
    }

    // Index of the first matching event, or -1.
    int IndexOf(const std::vector<FsEvent> &events, FsEvent::Kind kind, const std::string &name) {
        for (int i = 0; i < (int)events.size(); i++) {
            if (events[i].kind == kind && events[i].name == name) {
                return i;
            }
        }
        return -1;
    }
}

DLL_EXPORT int test_folderscanner(ITesting *t) {
    return kTR_Pass;
}

// An empty directory emits nothing at all.
DLL_EXPORT int test_folderscanner_emptydir(ITesting *t) {
    TempTree tree("empty");
    auto events = Walk(tree.root);
    TR_ASSERT(t, events.empty());
    return kTR_Pass;
}

// Files directly under root: onFile per file at depth 0, no dir events.
DLL_EXPORT int test_folderscanner_flat(ITesting *t) {
    TempTree tree("flat");
    tree.File("a.txt");
    tree.File("b.txt");

    auto events = Walk(tree.root);
    TR_ASSERT(t, Count(events, FsEvent::Kind::File) == 2);
    TR_ASSERT(t, Count(events, FsEvent::Kind::EnterDir) == 0);
    TR_ASSERT(t, Count(events, FsEvent::Kind::LeaveDir) == 0);
    for (const auto &e : events) {
        TR_ASSERT(t, e.depth == 0);
    }
    return kTR_Pass;
}

// Nested tree: a directory's child arrives between its enter and leave, one depth deeper.
DLL_EXPORT int test_folderscanner_nested(ITesting *t) {
    TempTree tree("nested");
    tree.File("a.txt");
    tree.Dir("sub");
    tree.File("sub/b.txt");

    auto events = Walk(tree.root);

    TR_ASSERT(t, Count(events, FsEvent::Kind::EnterDir) == 1);
    TR_ASSERT(t, Count(events, FsEvent::Kind::LeaveDir) == 1);
    TR_ASSERT(t, Count(events, FsEvent::Kind::File) == 2);   // a.txt + sub/b.txt

    int enterIdx = IndexOf(events, FsEvent::Kind::EnterDir, "sub");
    int leaveIdx = IndexOf(events, FsEvent::Kind::LeaveDir, "sub");
    int aIdx = IndexOf(events, FsEvent::Kind::File, "a.txt");
    int bIdx = IndexOf(events, FsEvent::Kind::File, "b.txt");

    TR_ASSERT(t, enterIdx >= 0 && leaveIdx >= 0 && aIdx >= 0 && bIdx >= 0);
    // enter precedes leave; the child is strictly between them.
    TR_ASSERT(t, enterIdx < leaveIdx);
    TR_ASSERT(t, bIdx > enterIdx && bIdx < leaveIdx);
    // Depth tracks nesting: root entries at 0, the subdir's child at 1.
    TR_ASSERT(t, events[enterIdx].depth == 0);
    TR_ASSERT(t, events[leaveIdx].depth == 0);
    TR_ASSERT(t, events[aIdx].depth == 0);
    TR_ASSERT(t, events[bIdx].depth == 1);
    return kTR_Pass;
}

// onEnterDir returning false vetoes descent: the directory's enter/leave still fire, but its children
// are never emitted. This is the seam future lazy expansion (FS-6) hangs on.
DLL_EXPORT int test_folderscanner_veto(ITesting *t) {
    TempTree tree("veto");
    tree.File("a.txt");
    tree.Dir("sub");
    tree.File("sub/b.txt");

    auto events = Walk(tree.root, {}, /*descend*/ false);

    TR_ASSERT(t, IndexOf(events, FsEvent::Kind::EnterDir, "sub") >= 0);
    TR_ASSERT(t, IndexOf(events, FsEvent::Kind::LeaveDir, "sub") >= 0);
    TR_ASSERT(t, IndexOf(events, FsEvent::Kind::File, "a.txt") >= 0);
    // The vetoed subtree produced nothing.
    TR_ASSERT(t, IndexOf(events, FsEvent::Kind::File, "b.txt") == -1);
    TR_ASSERT(t, Count(events, FsEvent::Kind::File) == 1);
    return kTR_Pass;
}

// maxDepth bounds descent: with maxDepth=2 the walk emits depth 0 and 1 only — deeper entries (and the
// file beneath them) never appear. This is the hard safety bound from FS-4 (open-bugs #4 stack-safety).
DLL_EXPORT int test_folderscanner_maxdepth(ITesting *t) {
    TempTree tree("maxdepth");
    tree.Dir("a/b/c");
    tree.File("a/b/c/deep.txt");

    FolderScanner::Options opts;
    opts.maxDepth = 2;
    auto events = Walk(tree.root, opts);

    TR_ASSERT(t, IndexOf(events, FsEvent::Kind::EnterDir, "a") >= 0);   // depth 0
    TR_ASSERT(t, IndexOf(events, FsEvent::Kind::EnterDir, "b") >= 0);   // depth 1
    TR_ASSERT(t, IndexOf(events, FsEvent::Kind::EnterDir, "c") == -1);  // depth 2: not entered
    TR_ASSERT(t, IndexOf(events, FsEvent::Kind::File, "deep.txt") == -1);
    for (const auto &e : events) {
        TR_ASSERT(t, e.depth < 2);
    }
    return kTR_Pass;
}

// A symlink cycle (loop/self -> loop) must NOT hang. With the default followSymlinks=false the symlinked
// directory is emitted once and never descended, so the walk is bounded — reaching this assert at all is
// the cycle-termination proof. This is the second failure mode in open-bugs #4.
DLL_EXPORT int test_folderscanner_symlink_no_follow(ITesting *t) {
    TempTree tree("symcycle");
    tree.Dir("loop");
    if (!tree.Symlink("loop", "loop/self")) {
        return kTR_Pass;   // filesystem refused symlink creation — nothing to validate
    }

    auto events = Walk(tree.root);   // default: followSymlinks = false

    TR_ASSERT(t, IndexOf(events, FsEvent::Kind::EnterDir, "loop") >= 0);
    TR_ASSERT(t, IndexOf(events, FsEvent::Kind::EnterDir, "self") >= 0);
    // loop + self, no repeats: had the cycle been followed, "self" would recur up to maxDepth.
    TR_ASSERT(t, Count(events, FsEvent::Kind::EnterDir) == 2);
    return kTR_Pass;
}

// followSymlinks=true descends through a (non-cyclic) symlinked directory: the file under it is reached
// via both the real path and the link. The default (false) reaches it once.
DLL_EXPORT int test_folderscanner_symlink_follow(ITesting *t) {
    TempTree tree("symfollow");
    tree.Dir("real");
    tree.File("real/x.txt");
    if (!tree.Symlink("real", "link")) {
        return kTR_Pass;
    }

    auto noFollow = Walk(tree.root);   // default false
    TR_ASSERT(t, Count(noFollow, FsEvent::Kind::File) == 1);   // only real/x.txt

    FolderScanner::Options opts;
    opts.followSymlinks = true;
    auto follow = Walk(tree.root, opts);
    TR_ASSERT(t, Count(follow, FsEvent::Kind::File) == 2);     // real/x.txt + link/x.txt
    return kTR_Pass;
}

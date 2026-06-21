//
// Created by gnilk on 17.06.26.
//
#include "FolderScanner.h"

using namespace gedit;
namespace fs = std::filesystem;

// The root itself is NOT emitted — the consumer already holds a node for it (matches the old
// Workspace::ReadFolderToNode contract, which iterated a root's children into an existing root
// node). Entries directly under root arrive at depth 0; each level of descent increments depth.
void FolderScanner::Scan(const fs::path &root, const Options &opts) {
    // Build the exclude matcher once and thread it through the recursion (not rebuilt per directory).
    FsFilter filter(opts.exclude);
    ScanDir(root, 0, opts, filter);
}

// True if `dir` is an ancestor of (or equal to) `refPath` — the reference lies within dir's subtree, so
// a reference-guided walk must descend here. lexically_relative gives a leading ".." for anything that
// is NOT below dir (siblings/cousins), which is how those are rejected.
static bool IsAncestorOf(const fs::path &dir, const fs::path &refPath) {
    auto rel = refPath.lexically_relative(dir);
    if (rel.empty()) {
        return false;
    }
    return *rel.begin() != "..";
}

void FolderScanner::ScanToReference(const fs::path &root, const fs::path &refPath, const Options &opts) {
    FsFilter filter(opts.exclude);
    ScanDirToReference(root, refPath.lexically_normal(), 0, opts, filter);
}

void FolderScanner::ScanDirToReference(const fs::path &dir, const fs::path &refPath, int depth, const Options &opts, const FsFilter &filter) {
    // No depth bound (unlike ScanDir): descent is constrained to the single chain of ancestors of
    // refPath, which is finite and acyclic, so it can't run away even past opts.maxDepth.
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(dir, ec)) {
        const auto &path = entry.path();
        if (filter.IsExcluded(path)) {
            continue;
        }
        if (fs::is_directory(path, ec)) {
            // Every directory is emitted (so siblings become frontier nodes), but only the one on the
            // path toward refPath is descended. onEnterDir's return is ignored here.
            const bool descend = IsAncestorOf(path.lexically_normal(), refPath);
            if (onEnterDir) {
                onEnterDir(path, depth);
            }
            if (descend) {
                ScanDirToReference(path, refPath, depth + 1, opts, filter);
            }
            if (onLeaveDir) {
                onLeaveDir(path, depth, descend);
            }
        } else if (fs::is_regular_file(path, ec)) {
            if (onFile) {
                onFile(path, depth);
            }
        }
    }
}

void FolderScanner::ScanDir(const fs::path &dir, int depth, const Options &opts, const FsFilter &filter) {
    // Hard depth bound: bounds path growth so a symlink cycle can't recurse until is_directory
    // throws ENAMETOOLONG. FS-4 adds the symlink-aware no-follow guard on top of this.
    if (depth >= opts.maxDepth) {
        return;
    }

    // Non-throwing overloads throughout: a leaf walk must never abort the process on an
    // unreadable directory or an over-long path.
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(dir, ec)) {
        const auto &path = entry.path();
        // Static exclude (FS-2): a matching entry is pruned entirely — no event, no descent. This is
        // where build dirs (cmake-build-debug, _deps, .git, ...) are kept out of memory, the build-dir
        // side of open-bugs #4.
        if (filter.IsExcluded(path)) {
            continue;
        }
        // is_symlink uses symlink_status (does NOT follow); is_directory DOES follow, so a symlink that
        // points at a directory reports as a directory here. Such an entry is still emitted via
        // onEnterDir, but it is only DESCENDED when followSymlinks is set — otherwise a cyclic link
        // (e.g. dir/self -> dir) is unbounded recursion, the second failure mode in open-bugs #4.
        const bool isSymlink = fs::is_symlink(path, ec);
        if (fs::is_directory(path, ec)) {
            bool descend = true;
            if (onEnterDir) {
                descend = onEnterDir(path, depth);
            }
            const bool mayFollow = opts.followSymlinks || !isSymlink;
            // fullyScanned is computed here (the call-site), not inside ScanDir: it captures
            // whether THIS directory was actually descended rather than whether the recursive
            // call itself encountered any guard (the depth guard fires at the TOP of ScanDir,
            // which would be invisible to the caller).
            const bool fullyScanned = descend && mayFollow && (depth + 1 < opts.maxDepth);
            if (descend && mayFollow) {
                ScanDir(path, depth + 1, opts, filter);
            }
            if (onLeaveDir) {
                onLeaveDir(path, depth, fullyScanned);
            }
        } else if (fs::is_regular_file(path, ec)) {
            if (onFile) {
                onFile(path, depth);
            }
        }
    }
}

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
    ScanDir(root, 0, opts);
}

void FolderScanner::ScanDir(const fs::path &dir, int depth, const Options &opts) {
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
            if (descend && mayFollow) {
                ScanDir(path, depth + 1, opts);
            }
            if (onLeaveDir) {
                onLeaveDir(path, depth);
            }
        } else if (fs::is_regular_file(path, ec)) {
            if (onFile) {
                onFile(path, depth);
            }
        }
    }
}

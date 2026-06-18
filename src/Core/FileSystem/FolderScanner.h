//
// Created by gnilk on 17.06.26.
//
// FolderScanner — a pure, recursive filesystem walk that emits plain-data discovery events
// through callbacks. It knows nothing about the editor data model (Node/Document/Workspace),
// so it is unit-testable standalone over a temp directory with no singleton in scope. The
// consumer owns the tree it builds and keeps its own parent stack (push on onEnterDir, pop on
// onLeaveDir). See docs/folder-scanner.md §3/§4 for why this is callbacks + plain data and not
// a template on the node type.
//
#ifndef GOATEDIT_FOLDERSCANNER_H
#define GOATEDIT_FOLDERSCANNER_H

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "FsFilter.h"

namespace gedit {

    // Plain-data vocabulary for a discovered entry. The eager-scan callbacks below intentionally do NOT
    // carry a kind (a non-followed symlinked directory still arrives via onEnterDir, just isn't
    // descended); kSymlink exists for the future batched FsEvent the monitor path will consume (FS-5).
    enum class FsEntryKind {
        kDirectory,
        kFile,
        kSymlink,
    };

    class FolderScanner {
    public:
        struct Options {
            int maxDepth = 8;                    // hard safety bound against cycles / pathological depth
            bool followSymlinks = false;         // FS-4: don't descend symlinked dirs (cycle safety)
            std::vector<std::string> exclude;    // FS-2: static prune (build dirs / dotfiles)
        };

    public:
        FolderScanner() = default;
        ~FolderScanner() = default;

        // Walk the entries of root (root itself is NOT emitted) and recurse into directories.
        void Scan(const std::filesystem::path &root, const Options &opts);

    public:
        // Return false => do NOT descend into this directory (the consumer's dynamic veto:
        // collapsed node / future lazy expansion). Returning true (or leaving it unset) descends.
        std::function<bool(const std::filesystem::path &path, int depth)> onEnterDir;
        std::function<void(const std::filesystem::path &path, int depth)> onFile;
        // fullyScanned is true when the scanner actually descended (consumer didn't veto,
        // not a non-followed symlink, and the depth bound wasn't hit). FS-6 uses this to
        // set Node::isScanned — false means the dir was emitted but not walked.
        std::function<void(const std::filesystem::path &path, int depth, bool fullyScanned)> onLeaveDir;

    private:
        void ScanDir(const std::filesystem::path &dir, int depth, const Options &opts, const FsFilter &filter);
    };
}

#endif //GOATEDIT_FOLDERSCANNER_H

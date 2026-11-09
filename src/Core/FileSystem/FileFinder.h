//
// FileFinder - every file of the project roots, for opening one by part of its name (docs/usability-round.md §3 b).
// The roots are walked off the UI thread, with what each root's ExclusionRules leave out left out - an ignored folder
// isn't entered. The list is a snapshot: a refresh walks again and swaps in a new one when done, so a reader always has
// a whole list, the last one. Plain: roots and rules in, paths out; the done handler is injected - no Editor here.
//
#ifndef GOATEDIT_FILEFINDER_H
#define GOATEDIT_FILEFINDER_H

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ExclusionRules.h"

namespace gedit {

    class FileFinder {
    public:
        using Ref = std::shared_ptr<FileFinder>;
        // Called on the walking thread when a new snapshot is in - post to the UI from it, don't draw
        using DoneHandler = std::function<void()>;

        struct Root {
            std::filesystem::path path;
            ExclusionRules::Ref rules = nullptr;    // null: everything
        };

        struct Options {
            int maxDepth = 32;
            size_t maxFiles = 200000;               // a walk stops there - a root that big isn't a project
        };

        // One walk's result - the files, below which root each is
        struct Snapshot {
            using Ref = std::shared_ptr<const Snapshot>;
            struct File {
                std::string path;
                size_t idxRoot = 0;
            };
            std::vector<Root> roots;
            std::vector<File> files;
            uint64_t generation = 0;
            bool isTruncated = false;               // stopped at maxFiles
        };

    public:
        FileFinder();
        explicit FileFinder(Options options);
        virtual ~FileFinder();

        static Ref Create();
        static Ref Create(Options options);

        // Walk these roots again, in the background. One walk at a time: a refresh asked for while one runs is done
        // when it ends.
        void Refresh(std::vector<Root> roots);
        // The last whole list - empty (generation 0) until the first walk is done
        Snapshot::Ref GetSnapshot() const;
        bool IsWalking() const {
            return isWalking.load();
        }
        void SetDoneHandler(DoneHandler newHandler);

        // Every file below 'root' that 'rules' leave in, depth-first; symlinked folders aren't followed (no cycles).
        // 'onFile' returns false to stop the walk, as does 'isStopping'. False when it was stopped.
        static bool Walk(const Root &root, const Options &options, const std::function<bool(const std::filesystem::path &)> &onFile,
                         const std::atomic<bool> &isStopping);

    private:
        void WalkThread(std::vector<Root> roots);
        void StopAndJoin();

    private:
        Options options;
        mutable std::mutex lock;
        Snapshot::Ref snapshot = std::make_shared<Snapshot>();
        std::vector<Root> pendingRoots;
        bool isRefreshPending = false;
        DoneHandler doneHandler = nullptr;
        std::thread walker;
        std::atomic<bool> isWalking = false;
        std::atomic<bool> isStopping = false;
    };

}

#endif //GOATEDIT_FILEFINDER_H

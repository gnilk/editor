//
// FileFinder - see FileFinder.h
//
#include "FileFinder.h"

#include <utility>

using namespace gedit;
namespace fs = std::filesystem;

FileFinder::FileFinder() = default;

FileFinder::FileFinder(Options options) : options(options) {
}

FileFinder::Ref FileFinder::Create() {
    return std::make_shared<FileFinder>();
}

FileFinder::Ref FileFinder::Create(Options options) {
    return std::make_shared<FileFinder>(options);
}

FileFinder::~FileFinder() {
    StopAndJoin();
}

// From one thread (the UI). The finished walker is joined outside the lock - its done handler may read the snapshot.
void FileFinder::Refresh(std::vector<Root> roots) {
    std::thread finished;
    {
        std::lock_guard guard(lock);
        if (isWalking.load()) {
            pendingRoots = std::move(roots);
            isRefreshPending = true;
            return;
        }
        finished = std::move(walker);
    }
    if (finished.joinable()) {
        finished.join();
    }
    std::lock_guard guard(lock);
    isWalking = true;
    walker = std::thread(&FileFinder::WalkThread, this, std::move(roots));
}

FileFinder::Snapshot::Ref FileFinder::GetSnapshot() const {
    std::lock_guard guard(lock);
    return snapshot;
}

void FileFinder::SetDoneHandler(DoneHandler newHandler) {
    std::lock_guard guard(lock);
    doneHandler = std::move(newHandler);
}

// Walks, swaps the snapshot in, and walks again while refreshes were asked for meanwhile
void FileFinder::WalkThread(std::vector<Root> roots) {
    while (true) {
        auto next = std::make_shared<Snapshot>();
        next->roots = roots;
        for (size_t idxRoot = 0; idxRoot < roots.size(); idxRoot++) {
            bool isWhole = Walk(roots[idxRoot], options, [&](const fs::path &path) {
                if (next->files.size() >= options.maxFiles) {
                    next->isTruncated = true;
                    return false;
                }
                next->files.push_back({path.string(), idxRoot});
                return true;
            }, isStopping);
            if (!isWhole) {
                break;
            }
        }
        if (isStopping.load()) {
            isWalking = false;
            return;
        }

        DoneHandler handler;
        {
            std::lock_guard guard(lock);
            next->generation = snapshot->generation + 1;
            snapshot = next;
            handler = doneHandler;
            if (!isRefreshPending) {
                isWalking = false;
            } else {
                roots = std::move(pendingRoots);
                isRefreshPending = false;
            }
        }
        if (handler) {
            handler();
        }
        if (!isWalking.load()) {
            return;
        }
    }
}

bool FileFinder::Walk(const Root &root, const Options &options, const std::function<bool(const fs::path &)> &onFile,
                      const std::atomic<bool> &isStopping) {
    struct Folder {
        fs::path path;
        int depth;
    };
    std::vector<Folder> folders = {{root.path, 0}};
    while (!folders.empty()) {
        auto folder = std::move(folders.back());
        folders.pop_back();
        std::error_code error;
        for (auto it = fs::directory_iterator(folder.path, fs::directory_options::skip_permission_denied, error);
             !error && (it != fs::directory_iterator()); it.increment(error)) {
            if (isStopping.load()) {
                return false;
            }
            auto &entry = *it;
            std::error_code statusError;
            auto linkStatus = entry.symlink_status(statusError);
            if (statusError) {
                continue;
            }
            if (fs::is_directory(linkStatus)) {
                if ((folder.depth + 1 < options.maxDepth) &&
                    ((root.rules == nullptr) || root.rules->IsIncluded(entry.path(), true))) {
                    folders.push_back({entry.path(), folder.depth + 1});
                }
                continue;
            }
            // A file, or a link to one - a link to a folder isn't followed
            if (!entry.is_regular_file(statusError) || statusError) {
                continue;
            }
            if ((root.rules != nullptr) && !root.rules->IsIncluded(entry.path(), false)) {
                continue;
            }
            if (!onFile(entry.path())) {
                return false;
            }
        }
    }
    return true;
}

void FileFinder::StopAndJoin() {
    isStopping = true;
    if (walker.joinable()) {
        walker.join();
    }
}

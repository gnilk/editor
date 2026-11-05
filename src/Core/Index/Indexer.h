//
// Feeds a CodeIndex from disk in the background (docs/code-index.md §4.4): walks the roots, then a pool of
// threads reads and scans each file. Never touches the UI; everything it learns goes into the index, which
// is searchable while this runs. No dependency on the Editor: the languages and excludes are handed in.
//

#ifndef GOATEDIT_INDEXER_H
#define GOATEDIT_INDEXER_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "CodeIndex.h"
#include "StructureScanner.h"
#include "Core/Language/LanguageBase.h"

namespace gedit {
    class Indexer {
    public:
        using Ref = std::shared_ptr<Indexer>;
        // Extension ('.cpp') -> language. Only these files are indexed.
        using Languages = std::unordered_map<std::string, LanguageBase::Ref>;

        struct Options {
            std::vector<std::string> exclude;           // workspace.exclude
            int maxDepth = 32;
            uintmax_t maxFileSize = 4 * 1024 * 1024;    // D5 - larger files are nearly always generated or vendored
            size_t numThreads = 0;                      // 0: cores - 1
        };

        struct Progress {
            size_t numDone = 0;
            size_t numTotal = 0;
            bool isIdle = true;
        };
    public:
        Indexer(CodeIndex::Ref index, Languages languages, Options options);
        virtual ~Indexer();

        static Ref Create(CodeIndex::Ref index, Languages languages, Options options) {
            return std::make_shared<Indexer>(std::move(index), std::move(languages), std::move(options));
        }

        // Index everything below a root; files closest to 'priorityPath' go first
        void AddRoot(const std::filesystem::path &root, const std::filesystem::path &priorityPath = {});
        // Read a file again - if it is gone, or no longer indexable, it leaves the index
        void Enqueue(const std::string &path);

        Progress GetProgress();
        // Block until the queue has drained (tests, gedit-bench)
        void WaitIdle();
        // Finish the files in hand and drop the rest. The destructor does this too.
        void Stop();
    private:
        struct WorkItem {
            bool isRoot = false;
            std::string path;
            std::string priorityPath;
        };
    private:
        void StartThreads();
        void WorkerLoop();
        bool TakeWork(WorkItem &item);
        void FinishWork(const WorkItem &item);
        void ScanRoot(const WorkItem &item);
        void IndexFile(const std::string &path, std::unordered_map<LanguageBase *, StructureScanner::Ptr> &scanners);
    private:
        CodeIndex::Ref index;
        Languages languages;
        Options options;

        std::mutex lock;
        std::condition_variable workAvailable;
        std::condition_variable idle;
        std::vector<std::thread> workers;
        std::deque<WorkItem> queue;
        std::unordered_set<std::string> queuedPaths;
        std::unordered_set<std::string> activePaths;    // two threads on one file could leave the older text in the index
        std::unordered_set<std::string> requeuedPaths;  // changed again while being read
        size_t numActive = 0;
        size_t numDone = 0;
        size_t numTotal = 0;
        bool isStopping = false;
    };
}

#endif //GOATEDIT_INDEXER_H

//
// Keeps one project root's change set current (docs/usability-round.md §0, config 'repomonitor'). Runs GitStatus
// when asked - the folder monitor asks when files change - and when a git operation changes '.git/index',
// '.git/HEAD' or '.git/logs/HEAD', which a thread of its own checks by modification time every interval: no process
// runs for that, and nothing at all while nothing changes.
//
// Asks are coalesced: one query runs at a time, and asks meanwhile make one more after it. The new change set is
// built on git's thread and handed over through 'postToMain', and only when it differs from the last.
//

#ifndef GOATEDIT_REPOMONITOR_H
#define GOATEDIT_REPOMONITOR_H

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "ChangeSet.h"

namespace gedit {
    class RepoMonitor : public std::enable_shared_from_this<RepoMonitor> {
    public:
        using Ref = std::shared_ptr<RepoMonitor>;
        using PostDelegate = std::function<void(std::function<void()>)>;
        // Main thread
        using ChangedDelegate = std::function<void(const ChangeSet::Ref &changeSet)>;
    public:
        RepoMonitor(std::filesystem::path rootPath, PostDelegate postToMain, std::chrono::milliseconds checkInterval);
        virtual ~RepoMonitor();

        static Ref Create(std::filesystem::path rootPath, PostDelegate postToMain,
                          std::chrono::milliseconds checkInterval = std::chrono::milliseconds(1000)) {
            return std::make_shared<RepoMonitor>(std::move(rootPath), std::move(postToMain), checkInterval);
        }

        void SetChangedHandler(ChangedDelegate newChangedHandler);
        // The first query, and the check thread
        void Start();
        void Stop();
        // Any thread
        void Refresh();

        // Main thread. Empty until the first answer.
        const ChangeSet::Ref &GetChangeSet() const {
            return changeSet;
        }
    private:
        void OnSnapshot(const GitStatus::Snapshot &snapshot);
        void Apply(const ChangeSet::Ref &newChangeSet);
        void CheckLoop();
        std::vector<std::filesystem::file_time_type> GitTimes(const std::filesystem::path &gitDir);
    private:
        const std::filesystem::path rootPath;
        const PostDelegate postToMain;
        const std::chrono::milliseconds checkInterval;
        ChangedDelegate onChanged = nullptr;

        std::mutex lock;
        std::condition_variable stopped;
        bool isStopping = false;
        bool isQueryRunning = false;
        bool isQueryPending = false;
        std::filesystem::path gitDir;
        std::vector<std::filesystem::file_time_type> knownTimes;     // of the git files, when the last query began
        ChangeSet::Ref latest = nullptr;    // the last one handed over, on git's thread - compared before posting
        std::thread checkThread;

        ChangeSet::Ref changeSet = nullptr; // main thread
    };
}

#endif //GOATEDIT_REPOMONITOR_H

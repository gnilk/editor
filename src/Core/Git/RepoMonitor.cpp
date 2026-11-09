//
// RepoMonitor - GitStatus on request and on changes to the git directory, coalesced
//

#include <system_error>

#include "RepoMonitor.h"

using namespace gedit;
namespace fs = std::filesystem;

RepoMonitor::RepoMonitor(fs::path rootPath, PostDelegate postToMain, std::chrono::milliseconds checkInterval) :
    rootPath(std::move(rootPath)),
    postToMain(std::move(postToMain)),
    checkInterval(checkInterval) {
}

RepoMonitor::~RepoMonitor() {
    Stop();
}

void RepoMonitor::SetChangedHandler(ChangedDelegate newChangedHandler) {
    onChanged = std::move(newChangedHandler);
}

void RepoMonitor::Start() {
    {
        std::lock_guard<std::mutex> guard(lock);
        if (checkThread.joinable()) {
            return;
        }
        isStopping = false;
        checkThread = std::thread([this]() {
            CheckLoop();
        });
    }
    Refresh();
}

void RepoMonitor::Stop() {
    {
        std::lock_guard<std::mutex> guard(lock);
        isStopping = true;
    }
    stopped.notify_all();
    if (checkThread.joinable() && (checkThread.get_id() != std::this_thread::get_id())) {
        checkThread.join();
    }
}

void RepoMonitor::Refresh() {
    std::weak_ptr<RepoMonitor> weakThis = weak_from_this();
    fs::path currentGitDir;
    {
        std::lock_guard<std::mutex> guard(lock);
        if (isStopping) {
            return;
        }
        if (isQueryRunning) {
            isQueryPending = true;
            return;
        }
        isQueryRunning = true;
        currentGitDir = gitDir;
    }
    // Before the query: a git operation while it runs is then seen as a change by the next check
    if (!currentGitDir.empty()) {
        auto times = GitTimes(currentGitDir);
        std::lock_guard<std::mutex> guard(lock);
        knownTimes = std::move(times);
    }
    // The query outlives nothing it needs: an answer for a monitor that is gone is dropped
    GitStatus::Query(rootPath, [weakThis](const GitStatus::Snapshot &snapshot) {
        if (auto self = weakThis.lock()) {
            self->OnSnapshot(snapshot);
        }
    });
}

// Git's thread
void RepoMonitor::OnSnapshot(const GitStatus::Snapshot &snapshot) {
    // A repository whose status failed keeps what is shown; a folder that isn't one shows nothing
    bool isUsable = snapshot.error.empty() || !snapshot.isRepository;
    ChangeSet::Ref newChangeSet = isUsable ? ChangeSet::Create(snapshot, rootPath) : nullptr;

    // The first answer says where the git directory is
    std::vector<fs::file_time_type> firstTimes;
    if (!snapshot.gitDir.empty()) {
        firstTimes = GitTimes(snapshot.gitDir);
    }

    bool isPending = false;
    bool isChanged = false;
    {
        std::lock_guard<std::mutex> guard(lock);
        if (gitDir != snapshot.gitDir) {
            gitDir = snapshot.gitDir;
            knownTimes = std::move(firstTimes);
        }
        if ((newChangeSet != nullptr) && ((latest == nullptr) || !latest->IsSameAs(*newChangeSet))) {
            latest = newChangeSet;
            isChanged = true;
        }
        isPending = isQueryPending;
        isQueryPending = false;
        isQueryRunning = false;
    }

    if (isChanged && (postToMain != nullptr)) {
        std::weak_ptr<RepoMonitor> weakThis = weak_from_this();
        postToMain([weakThis, newChangeSet]() {
            if (auto self = weakThis.lock()) {
                self->Apply(newChangeSet);
            }
        });
    }
    if (isPending) {
        Refresh();
    }
}

// Main thread
void RepoMonitor::Apply(const ChangeSet::Ref &newChangeSet) {
    changeSet = newChangeSet;
    if (onChanged != nullptr) {
        onChanged(changeSet);
    }
}

void RepoMonitor::CheckLoop() {
    std::unique_lock<std::mutex> guard(lock);
    while (!isStopping) {
        stopped.wait_for(guard, checkInterval, [this]() {
            return isStopping;
        });
        if (isStopping || gitDir.empty() || knownTimes.empty()) {
            continue;
        }
        auto currentGitDir = gitDir;
        guard.unlock();
        auto times = GitTimes(currentGitDir);
        guard.lock();

        if ((currentGitDir == gitDir) && (times != knownTimes)) {
            knownTimes = times;
            guard.unlock();
            Refresh();
            guard.lock();
        }
    }
}

// What a git operation rewrites: the index (add, commit, reset, checkout), HEAD (switch) and its log (commit, merge,
// rebase - anything that moves the branch HEAD names). A missing file reads as the minimum time.
std::vector<fs::file_time_type> RepoMonitor::GitTimes(const fs::path &dir) {
    std::vector<fs::file_time_type> times;
    for (auto name : {"index", "HEAD", "logs/HEAD"}) {
        std::error_code error;
        auto time = fs::last_write_time(dir / name, error);
        times.push_back(error ? fs::file_time_type::min() : time);
    }
    return times;
}

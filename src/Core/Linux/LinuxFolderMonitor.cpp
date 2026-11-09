//
// Created by gnilk on 31.08.23.
//
// inotify backend - docs/foldermonitor_fixes.md §2.5. The OS side only says "something happened at this
// path"; the base collects dirty folders and the workspace reconciles them, so renames, atomic saves and
// coalesced events need no special handling here.
//
#include <sys/inotify.h>
#include <climits>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <cstring>
#include <pthread.h>

#include "LinuxFolderMonitor.h"
#include "Core/CompileTimeConfig.h"

using namespace gedit;

// Folders: entries added / removed / renamed in them, files in them written, the folder itself going away.
// No IN_ONESHOT - a one-shot watch reports once and then goes deaf.
static constexpr uint32_t kWatchMask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE |
                                       IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR;

FolderMonitor::MonitorPoint::Ref LinuxFolderMonitor::CreateMonitorPoint(const std::filesystem::path &pathToMonitor) {
    return LinuxFolderMonitorPoint::Create(pathToMonitor);
}

LinuxFolderMonitorPoint::~LinuxFolderMonitorPoint() {
    Stop();
}

bool LinuxFolderMonitorPoint::Start() {
    if (IsRunning()) {
        return true;
    }
    logger = gnilk::Logger::GetLogger("FolderMonitor");

    iNotifyFd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (iNotifyFd < 0) {
        logger->Error("inotify_init1 failed: %s", strerror(errno));
        return false;
    }

    WatchTree(pathToMonitor);
    if (watches.empty()) {
        logger->Error("Unable to watch: %s", pathToMonitor.c_str());
        close(iNotifyFd);
        iNotifyFd = -1;
        return false;
    }
    logger->Debug("Monitoring %zu folders below: %s", watches.size(), pathToMonitor.c_str());

    isQuitRequested = false;
    isRunning = true;
    eventThread = std::thread([this]() {
        pthread_setname_np(pthread_self(), "LnxFldMon");
        EventThread();
    });
    return true;
}

bool LinuxFolderMonitorPoint::Stop() {
    // Events still in flight are dropped by the base from here on
    isRunning = false;
    isQuitRequested = true;
    if (eventThread.joinable()) {
        eventThread.join();
    }
    // Closing the descriptor removes every watch
    if (iNotifyFd >= 0) {
        close(iNotifyFd);
        iNotifyFd = -1;
    }
    watches.clear();
    return true;
}

void LinuxFolderMonitorPoint::EventThread() {
    // Room for at least one event with the longest name, as inotify(7) asks
    alignas(struct inotify_event) char buffer[16 * (sizeof(struct inotify_event) + NAME_MAX + 1)];
    struct pollfd pfd = { iNotifyFd, POLLIN, 0 };

    while (!isQuitRequested) {
        auto pollResult = poll(&pfd, 1, GEDIT_DEFAULT_POLL_TMO_MS);
        if (pollResult < 0) {
            if (errno == EINTR) {
                continue;
            }
            logger->Error("poll failed: %s", strerror(errno));
            break;
        }
        if ((pollResult == 0) || !(pfd.revents & POLLIN)) {
            continue;
        }

        auto numRead = read(iNotifyFd, buffer, sizeof(buffer));
        if (numRead < 0) {
            if ((errno == EAGAIN) || (errno == EINTR)) {
                continue;
            }
            logger->Error("read from inotify failed: %s", strerror(errno));
            break;
        }
        for (char *ptr = buffer; ptr < buffer + numRead; ) {
            auto event = reinterpret_cast<const struct inotify_event *>(ptr);
            ProcessEvent(event);
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
}

void LinuxFolderMonitorPoint::ProcessEvent(const struct inotify_event *event) {
    if (event->mask & IN_Q_OVERFLOW) {
        OnLostEvents();
        return;
    }

    auto itWatch = watches.find(event->wd);
    if (itWatch == watches.end()) {
        return;
    }
    // The kernel removed the watch (folder deleted, or unmounted) - its wd may be handed out again
    if (event->mask & IN_IGNORED) {
        watches.erase(itWatch);
        return;
    }
    const auto folder = itWatch->second;

    // The watched folder itself was deleted or moved. Its parent's watch reports that too; only the root
    // has no parent watch, and then everything loaded may be stale.
    if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) {
        if (folder == pathToMonitor) {
            OnLostEvents();
        }
        return;
    }
    if (event->len == 0) {
        return;
    }

    auto path = folder / event->name;
    // A folder that arrived: watch it, and what is already in it. Entries created before the watch existed
    // are not lost - the base marks the new folder itself dirty and the reconcile reads it.
    if ((event->mask & IN_ISDIR) && (event->mask & (IN_CREATE | IN_MOVED_TO))) {
        if (IsFolderIncluded(path)) {
            WatchTree(path);
        }
    }
    OnRawEvent(path);
}

void LinuxFolderMonitorPoint::WatchTree(const std::filesystem::path &folder) {
    AddWatch(folder);

    std::error_code ec;
    auto options = std::filesystem::directory_options::skip_permission_denied;
    auto it = std::filesystem::recursive_directory_iterator(folder, options, ec);
    for (; !ec && (it != std::filesystem::recursive_directory_iterator()); it.increment(ec)) {
        std::error_code ecEntry;
        // Not into symlinked folders - the iterator doesn't follow them, and neither do we
        if (!it->is_directory(ecEntry) || it->is_symlink(ecEntry)) {
            continue;
        }
        if (!IsFolderIncluded(it->path())) {
            // Don't even walk it - a build tree or _deps can hold thousands of folders
            it.disable_recursion_pending();
            continue;
        }
        AddWatch(it->path());
    }
}

void LinuxFolderMonitorPoint::AddWatch(const std::filesystem::path &folder) {
    auto wd = inotify_add_watch(iNotifyFd, folder.c_str(), kWatchMask);
    if (wd < 0) {
        // ENOSPC: out of watches (fs.inotify.max_user_watches). Say it once, keep what we have.
        if ((errno == ENOSPC) && !isWatchLimitReported) {
            isWatchLimitReported = true;
            logger->Warning("inotify watch limit reached - changes below some folders are not seen (fs.inotify.max_user_watches)");
        } else if (errno != ENOSPC) {
            logger->Debug("Unable to watch '%s': %s", folder.c_str(), strerror(errno));
        }
        return;
    }
    // The same folder gives the same wd (the mask is replaced) - the map stays one entry per folder
    watches[wd] = folder;
}

bool LinuxFolderMonitorPoint::IsFolderIncluded(const std::filesystem::path &folder) const {
    auto &rules = GetExclusionRules();
    return (rules == nullptr) || rules->IsIncluded(folder, true);
}

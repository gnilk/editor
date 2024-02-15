//
// Created by gnilk on 31.08.23.
//
#include <stdio.h>
#include <sys/inotify.h>
#include <climits>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <cstring>

#include "LinuxFolderMonitor.h"
#include "Core/Config/Config.h"
#include "Core/CompileTimeConfig.h"
#include "Core/Editor.h"

using namespace gedit;

FolderMonitor::MonitorPoint::Ref LinuxFolderMonitor::CreateMonitorPoint(const std::filesystem::path &pathToMonitor, EventDelegate handler) {
    auto monitor = LinuxFolderMonitorPoint::Create(pathToMonitor);

    monitor->SetEventHandler(handler);

    AddMonitor(monitor);

    return monitor;
}

LinuxFolderMonitorPoint::~LinuxFolderMonitorPoint() {
    if (isRunning) {
        Stop();
    }
}

bool LinuxFolderMonitorPoint::Start() {
    if (IsRunning()) {
        return true;
    }

    logger = gnilk::Logger::GetLogger("FolderMonitor");
    // Non-blocking...
    iNotifyFd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (iNotifyFd < 0) {

        return false;
    }

    scanThread = std::thread([this](){
        ScanThread();
    });
    isRunning = true;
    return true;
}

bool LinuxFolderMonitorPoint::Stop() {
    if (!IsRunning()) {
        return false;
    }
    bQuitThread = true;
    scanThread.join();
    isRunning = false;

    // FIXME: Clean up watchers..
    return true;
}

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))


void LinuxFolderMonitorPoint::ScanThread() {
    ScanForDirectories(std::filesystem::path(pathToMonitor));
    StartWatchers();

    nfds_t nfds;
    struct pollfd fds[1];

    nfds = 1;
    fds[0].fd = iNotifyFd;
    fds[0].events = POLLIN;

    char buf[BUF_LEN];

    while(!bQuitThread) {
        // FIXME: Verify this
        auto poll_num = poll(fds, nfds, GEDIT_DEFAULT_POLL_TMO_MS);
        if (poll_num == -1) {
            if (errno == EINTR) continue;
            logger->Error("poll error=%d:%s", errno, strerror(errno));
            break;
        }
        if (poll_num == 0) continue;
        if (!(fds[0].revents & POLLIN)) continue;

        auto numRead = read(iNotifyFd, buf, BUF_LEN);
        if (numRead == 0) {
            continue;
        }
        if (numRead < 0) {
            logger->Error("read from iNotifyFD returned: %d", (int)numRead);
            break;
        }

        // Process the event structures we just got...
        for(auto p = buf; p<buf + numRead;) {
            struct inotify_event *event = (struct inotify_event *)p;
            ProcessEvent(event);
            p += sizeof(struct inotify_event) + event->len;
        }

        // Yield thread
        // Consider adding a sleep here - relax the load on the inotify system
        std::this_thread::yield();
    }

    close(iNotifyFd);
    bQuitThread = true;
    isRunning = false;
}

// Process one event structure at the time from inotify
void LinuxFolderMonitorPoint::ProcessEvent(struct inotify_event *event) {
    printf("mask: 0x%x\n",event->mask);

    if (!((event->mask & IN_CREATE) || (event->mask & IN_DELETE)))
        return;
   if (event->len <= 0)
       return;

    auto itWatcher = watchers.find(event->wd);
    if (itWatcher == watchers.end())
        return;

    auto path = itWatcher->second;
    path = path / event->name;
    if ((event->mask & IN_CREATE) && (AddMonitorItem(path))) {
        StartWatchers();
    }
    OnFSEvent(path.string(), (event->mask & IN_CREATE)?FolderMonitor::kCreated : FolderMonitor::kRemoved);
}

void LinuxFolderMonitorPoint::OnFSEvent(const std::string &fullPathName, FolderMonitor::kChangeFlags flags) {
    DispatchEvent(fullPathName, flags);
}

void LinuxFolderMonitorPoint::ScanForDirectories(std::filesystem::path path) {
    monitorList.clear();

    AddMonitorItem(path);

    auto cfgMonitor = Config::Instance().GetNode("foldermonitor");
    auto foldersToExclude = cfgMonitor.GetSequenceOfStr("exclude");

    for(const auto &dirEntry : std::filesystem::recursive_directory_iterator(path)) {
        if (!is_directory(dirEntry.path())) continue;

        auto itFound = std::find_if(foldersToExclude.begin(), foldersToExclude.end(), [&dirEntry](const std::string &f){
            return (dirEntry.path().string().find(f) != std::string::npos);

        });
        if (itFound != foldersToExclude.end()) {
            // logger->Debug("Excluding: %s", dirEntry.path().c_str());
            continue;
        }
        AddMonitorItem(dirEntry.path());
    }
}

bool LinuxFolderMonitorPoint::AddMonitorItem(std::filesystem::path pathToFolder) {
    if (!is_directory(pathToFolder)) {
        logger->Debug("AddMonitorItem, not a directory: %s\n",pathToFolder.c_str());
        return false;
    }
    LinuxMonitorItem item;
    item.wd = -1;
    item.path = pathToFolder;
    monitorList.push_back(item);
    return true;
}

void LinuxFolderMonitorPoint::StartWatchers() {


    for(auto &item : monitorList) {
        if (item.wd >= 0) {
            continue;
        }

        item.wd = inotify_add_watch(iNotifyFd, item.path.c_str(), IN_ONESHOT | IN_CREATE | IN_DELETE);
        if (item.wd < 0) {
            logger->Error("Failed watcher on item: %s\n", item.path.c_str());
            continue;
        }
        logger->Info("Monitoring: %s", item.path.c_str());
        watchers[item.wd] = item.path;
    }
}

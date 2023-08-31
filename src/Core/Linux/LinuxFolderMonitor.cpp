//
// Created by gnilk on 31.08.23.
//
#include <stdio.h>
#include <sys/inotify.h>
#include <climits>
#include <unistd.h>
#include <stdlib.h>

#include "LinuxFolderMonitor.h"

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
    iNotifyFd = inotify_init1(IN_CLOEXEC);
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

    char buf[BUF_LEN];

    while(!bQuitThread) {
        auto numRead = read(iNotifyFd, buf, BUF_LEN);
        if (numRead == 0) {
            continue;
        }
        if (numRead < 0) {
            logger->Error("read from iNotifyFD returned: %d", (int)numRead);
            break;
        }

        for(auto p = buf; p<buf + numRead;) {
            struct inotify_event *event = (struct inotify_event *)p;
            ProcessEvent(event);
            p += sizeof(struct inotify_event) + event->len;
        }
    }
    bQuitThread = true;
    isRunning = false;
}
void LinuxFolderMonitorPoint::ProcessEvent(struct inotify_event *event) {
    if (event->mask & IN_CREATE) {
        if (event->len > 0) {
            auto itWatcher = watchers.find(event->wd);
            if (itWatcher != watchers.end()) {
                auto path = itWatcher->second;
                path = path / event->name;
                if (AddMonitorItem(path)) {
                    StartWatchers();
                }

                OnFSEvent(path.string(), FolderMonitor::kChangeFlags::kCreated);
            }
        }
    } else if (event->mask & IN_DELETE) {
        if (event->len > 0) {
            if (event->len > 0) {
                auto itWatcher = watchers.find(event->wd);
                if (itWatcher != watchers.end()) {
                    auto path = itWatcher->second;
                    path = path / event->name;
                    OnFSEvent(path.string(), FolderMonitor::kChangeFlags::kRemoved);
                }
            }
        }
    } else {
        // We don't really handle any more types
    }
}

void LinuxFolderMonitorPoint::OnFSEvent(const std::string &path, FolderMonitor::kChangeFlags flags) {
    DispatchEvent(path, flags);
}




void LinuxFolderMonitorPoint::ScanForDirectories(std::filesystem::path path) {
    monitorList.clear();

    AddMonitorItem(path);

    for(const auto &dirEntry : std::filesystem::recursive_directory_iterator(path)) {
        if (!is_directory(dirEntry.path())) continue;
        printf("%s\n", dirEntry.path().c_str());
        AddMonitorItem(dirEntry.path());
    }

}
bool LinuxFolderMonitorPoint::AddMonitorItem(std::filesystem::path path) {
    if (!is_directory(path)) {
        logger->Debug("AddMonitorItem, not a directory: %s\n",path.c_str());
        return false;
    }
    LinuxMonitorItem item;
    item.wd = -1;
    item.path = path;
    monitorList.push_back(item);
    return true;
}

void LinuxFolderMonitorPoint::StartWatchers() {
    for(auto &item : monitorList) {
        if (item.wd >= 0) {
            continue;
        }
        // FIXME: No need to pass in 'IN_ALL_EVENTS'
        item.wd = inotify_add_watch(iNotifyFd, item.path.c_str(), IN_ALL_EVENTS);
        if (item.wd < 0) {
            logger->Debug("Failed watcher on item: %s\n", item.path.c_str());
        }
        printf("Ok, monitoring: %s\n", item.path.c_str());
        watchers[item.wd] = item.path;
    }
}

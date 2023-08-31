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

bool LinuxFolderMonitorPoint::Start() {

}

bool LinuxFolderMonitorPoint::Stop() {

}

void LinuxFolderMonitorPoint::ScanForDirectories(std::filesystem::path path) {
    AddMonitorItem(path);

    for(const auto &dirEntry : std::filesystem::recursive_directory_iterator(path)) {
        if (!is_directory(dirEntry.path())) continue;
        printf("%s\n", dirEntry.path().c_str());
        AddMonitorItem(dirEntry.path());
    }

}
void LinuxFolderMonitorPoint::AddMonitorItem(std::filesystem::path path) {
    if (!is_directory(path)) {
        printf("AddMonitorItem, not a directory: %s\n",path.c_str());
        return;
    }
    LinuxMonitorItem item;
    item.wd = -1;
    item.path = path;
    monitorList.push_back(item);
}

void LinuxFolderMonitorPoint::StartWatchers() {
    for(auto &item : monitorList) {
        if (item.wd >= 0) {
            continue;
        }
        item.wd = inotify_add_watch(iNotifyFd, item.path.c_str(), IN_ALL_EVENTS);
        if (item.wd < 0) {
            printf("Failed watcher on item: %s\n", item.path.c_str());
        }
        printf("Ok, monitoring: %s\n", item.path.c_str());
        watchers[item.wd] = item.path;
    }
}

void LinuxFolderMonitorPoint::OnFSEvent(const std::string &path, FolderMonitor::kChangeFlags flags) {

}

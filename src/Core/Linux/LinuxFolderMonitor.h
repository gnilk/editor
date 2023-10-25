//
// Created by gnilk on 31.08.23.
//

#ifndef GOATEDIT_LINUXFOLDERMONITOR_H
#define GOATEDIT_LINUXFOLDERMONITOR_H

#include <memory>
#include <string>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <sys/inotify.h>


#include "logger.h"

#include "Core/FolderMonitor.h"

namespace gedit {
    // Monitoring the folder and all sub-folders (incl. files) below the monitor point
    class LinuxFolderMonitorPoint : public FolderMonitor::MonitorPoint {
    public:
        using Ref = std::shared_ptr<LinuxFolderMonitorPoint>;

        explicit LinuxFolderMonitorPoint(const std::string &monitorPath) : FolderMonitor::MonitorPoint(monitorPath) {

        }
        virtual ~LinuxFolderMonitorPoint();

        static Ref Create(const std::string &pathToMonitor) {
            return std::make_shared<LinuxFolderMonitorPoint>(pathToMonitor);
        }

        // Start/Stop monitoring
        bool Start() override;
        bool Stop() override;

    protected:
        LinuxFolderMonitorPoint() = default;

        void ScanForDirectories(std::filesystem::path path);
        bool AddMonitorItem(std::filesystem::path pathToFolder);
        void StartWatchers();
        void ScanThread();
        void ProcessEvent(struct inotify_event *event);
        void OnFSEvent(const std::string &fullPathName, FolderMonitor::kChangeFlags flags);
    protected:
        struct LinuxMonitorItem {
            int wd = -1;
            std::filesystem::path path;
        };
    protected:
        gnilk::ILogger *logger = nullptr;
        int iNotifyFd = -1;
        std::vector<LinuxMonitorItem> monitorList;
        std::unordered_map<int, std::filesystem::path> watchers;
        std::thread scanThread;
        std::atomic_bool bQuitThread = false;
    };

    class LinuxFolderMonitor : public FolderMonitor {
    public:
        LinuxFolderMonitor() = default;
        virtual ~LinuxFolderMonitor() = default;

        MonitorPoint::Ref CreateMonitorPoint(const std::filesystem::path &pathToMonitor, EventDelegate handler) override;
    };
}


#endif //GOATEDIT_LINUXFOLDERMONITOR_H

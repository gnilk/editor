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

#include "logger.h"

#include "Core/FolderMonitor.h"

namespace gedit {
    class LinuxFolderMonitorPoint : public FolderMonitor::MonitorPoint {
    public:
        using Ref = std::shared_ptr<LinuxFolderMonitorPoint>;

        explicit LinuxFolderMonitorPoint(const std::string &pathToMonitor) : FolderMonitor::MonitorPoint(pathToMonitor) {

        }
        virtual ~LinuxFolderMonitorPoint();
        static Ref Create(const std::string &pathToMonitor) {
            return std::make_shared<LinuxFolderMonitorPoint>(pathToMonitor);
        }

        bool Start() override;
        bool Stop() override;

        void OnFSEvent(const std::string &path, FolderMonitor::kChangeFlags flags);
    protected:
        LinuxFolderMonitorPoint() = default;

        void ScanForDirectories(std::filesystem::path path);
        void AddMonitorItem(std::filesystem::path);
        void StartWatchers();
        void ScanThread();
    protected:
        struct LinuxMonitorItem {
            int wd = -1;
            std::filesystem::path path;
        };
    protected:
        int iNotifyFd = -1;
        std::mutex dataLock;
        std::vector<LinuxMonitorItem> monitorList;
        std::unordered_map<int, std::filesystem::path> watchers;

    };

    class LinuxFolderMonitor : public FolderMonitor {
    public:
        LinuxFolderMonitor() = default;
        virtual ~LinuxFolderMonitor() = default;

        MonitorPoint::Ref CreateMonitorPoint(const std::filesystem::path &pathToMonitor, EventDelegate handler) override;
    };
}


#endif //GOATEDIT_LINUXFOLDERMONITOR_H

//
// Created by gnilk on 31.08.23.
//

#ifndef GOATEDIT_LINUXFOLDERMONITOR_H
#define GOATEDIT_LINUXFOLDERMONITOR_H

#include <memory>
#include <string>
#include <filesystem>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <sys/inotify.h>

#include "logger.h"

#include "Core/FolderMonitor.h"

namespace gedit {
    // inotify backend (docs/foldermonitor_fixes.md §2.5): one watch per non-excluded folder below the root.
    // Only reports paths - the base turns them into dirty folders, which the workspace reconciles.
    class LinuxFolderMonitorPoint : public FolderMonitor::MonitorPoint {
    public:
        using Ref = std::shared_ptr<LinuxFolderMonitorPoint>;

        explicit LinuxFolderMonitorPoint(const std::filesystem::path &monitorPath) : FolderMonitor::MonitorPoint(monitorPath) {
        }
        ~LinuxFolderMonitorPoint() override;

        static Ref Create(const std::filesystem::path &pathToMonitor) {
            return std::make_shared<LinuxFolderMonitorPoint>(pathToMonitor);
        }

        // Start watches the tree before it returns - the monitor is live from then on, like the macOS stream
        bool Start() override;
        // Main thread; joins the event thread
        bool Stop() override;

    protected:
        LinuxFolderMonitorPoint() = default;

        void EventThread();
        void ProcessEvent(const struct inotify_event *event);
        // Watch 'folder' and every non-excluded folder below it
        void WatchTree(const std::filesystem::path &folder);
        void AddWatch(const std::filesystem::path &folder);
        bool IsFolderIncluded(const std::filesystem::path &folder) const;

    protected:
        gnilk::ILogger *logger = nullptr;
        int iNotifyFd = -1;
        // wd -> folder. Filled by Start, then touched only by the event thread.
        std::unordered_map<int, std::filesystem::path> watches;
        bool isWatchLimitReported = false;
        std::thread eventThread;
        std::atomic_bool isQuitRequested = false;
    };

    class LinuxFolderMonitor : public FolderMonitor {
    public:
        LinuxFolderMonitor() = default;
        ~LinuxFolderMonitor() override = default;

        MonitorPoint::Ref CreateMonitorPoint(const std::filesystem::path &pathToMonitor) override;
    };
}


#endif //GOATEDIT_LINUXFOLDERMONITOR_H

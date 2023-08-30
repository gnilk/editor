//
// Created by gnilk on 29.08.23.
//

#ifndef GOATEDIT_MACOSFOLDERMONITOR_H
#define GOATEDIT_MACOSFOLDERMONITOR_H

#include <memory>
#include <string>
#include <filesystem>
#include <mutex>

#include <CoreServices/CoreServices.h>

#include "Core/FolderMonitor.h"

namespace gedit {

    class MacOSFolderMonitorPoint : public FolderMonitor::MonitorPoint {
    public:
        using Ref = std::shared_ptr<MacOSFolderMonitorPoint>;
    public:
        explicit MacOSFolderMonitorPoint(const std::string &pathToMonitor) : FolderMonitor::MonitorPoint(pathToMonitor) {

        }
        virtual ~MacOSFolderMonitorPoint();
        static Ref Create(const std::string &pathToMonitor) {
            return std::make_shared<MacOSFolderMonitorPoint>(pathToMonitor);
        }

        bool Start() override;
        bool Stop() override;

        void OnFSEvent(const std::string &path, FolderMonitor::kChangeFlags flags);
    protected:
        MacOSFolderMonitorPoint() = default;
    protected:
        FSEventStreamContext fsContext = {};
        CFStringRef cfPathToWatch = {};
        CFArrayRef cfPathsToWatch = {};

        CFMutableArrayRef cfPathsToExclude = {};
        FSEventStreamRef fsEventStream = {};

    };


    class MacOSFolderMonitor : public FolderMonitor {
    public:
        MacOSFolderMonitor() = default;
        virtual ~MacOSFolderMonitor() = default;

        MonitorPoint::Ref CreateMonitorPoint(const std::filesystem::path &pathToMonitor, EventDelegate handler) override;
    };
}


#endif //GOATEDIT_MACOSFOLDERMONITOR_H

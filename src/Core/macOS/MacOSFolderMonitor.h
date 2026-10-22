//
// Created by gnilk on 29.08.23.
//

#ifndef GOATEDIT_MACOSFOLDERMONITOR_H
#define GOATEDIT_MACOSFOLDERMONITOR_H

#include <memory>
#include <string>
#include <filesystem>

#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>

#include "Core/FolderMonitor.h"

namespace gedit {

    //
    // FSEvents backend. The stream runs on a private serial dispatch queue (not the main queue - nothing
    // depends on SDL pumping GCD) and feeds every reported path to OnRawEvent; the flags are not trusted
    // beyond "events were lost". The base filters, coalesces and posts to the main thread.
    //
    class MacOSFolderMonitorPoint : public FolderMonitor::MonitorPoint {
    public:
        using Ref = std::shared_ptr<MacOSFolderMonitorPoint>;
    public:
        explicit MacOSFolderMonitorPoint(const std::filesystem::path &pathToMonitor) : FolderMonitor::MonitorPoint(pathToMonitor) {

        }
        virtual ~MacOSFolderMonitorPoint();
        static Ref Create(const std::filesystem::path &pathToMonitor) {
            return std::make_shared<MacOSFolderMonitorPoint>(pathToMonitor);
        }

        bool Start() override;
        bool Stop() override;

        // Called on the stream's queue (by the C callback)
        void OnFSEvents(size_t numEvents, const char **paths, const FSEventStreamEventFlags eventFlags[]);
    protected:
        MacOSFolderMonitorPoint() = default;
        // The excluded top-level folders as concrete paths (FSEvents takes no globs, at most 8)
        void SetOSExclusionPaths();
    protected:
        FSEventStreamContext fsContext = {};
        FSEventStreamRef fsEventStream = nullptr;
        dispatch_queue_t dispatchQueue = nullptr;
    };


    class MacOSFolderMonitor : public FolderMonitor {
    public:
        MacOSFolderMonitor() = default;
        virtual ~MacOSFolderMonitor() = default;

        MonitorPoint::Ref CreateMonitorPoint(const std::filesystem::path &pathToMonitor) override;
    };
}


#endif //GOATEDIT_MACOSFOLDERMONITOR_H

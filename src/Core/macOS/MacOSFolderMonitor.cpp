//
// Created by gnilk on 29.08.23.
// TODO: Refactor this - we don't really want this API, instead a workspace root-node should be able to create a specific folder-monitor with exclusion paths
//       Thus, we should:
//           auto folderMonitor = RuntimeConfig::Instance().GetFolderMonitor();
//           auto monitoringRef = folderMonitor.CreateMonitoring(<path>, <exclusion>, <callback>);
//           monitoringRef->Start();
//
#include <CoreServices/CoreServices.h>
#include <mutex>
#include "logger.h"
#include "MacOSFolderMonitor.h"

using namespace gedit;

MacOSFolderMonitor &MacOSFolderMonitor::Instance() {
    static MacOSFolderMonitor glbMonitor;
    return glbMonitor;
}

static void fsNotifyTrampoline(

        ConstFSEventStreamRef streamRef,
        void *clientCallBackInfo,
        size_t numEvents,
        void *eventPaths,
        const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId eventIds[]

){

    if (clientCallBackInfo != nullptr) {
        auto pThis = static_cast<MacOSFolderMonitor *>(clientCallBackInfo);
        const char **paths = (const char **)eventPaths;

        for(auto i=0;i<numEvents;i++) {
            std::string path(paths[i]);
            // FIXME: Translate events here
            FolderMonitorBase::kChangeFlags flags = {};

            if (eventFlags[i] & kFSEventStreamEventFlagItemCreated) {
                flags |= FolderMonitorBase::kChangeFlags::kCreated;
            }
            if (eventFlags[i] & kFSEventStreamEventFlagItemModified) {
                flags |= FolderMonitorBase::kChangeFlags::kModified;
            }
            if (eventFlags[i] & kFSEventStreamEventFlagItemRemoved) {
                flags |= FolderMonitorBase::kChangeFlags::kRemoved;
            }
            if (eventFlags[i] & kFSEventStreamEventFlagItemRenamed) {
                flags |= FolderMonitorBase::kChangeFlags::kRenamed;
            }

            pThis->OnFSEvent(path, flags);
        }
    }

}


bool MacOSFolderMonitor::Start() {
    if (isRunning) {
        return true;
    }

    if (folderList.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> guard(dataGuard);

    // Setup context so we can jump into the class
    FSEventStreamContext context = {};
    context.info = this;

    CFAbsoluteTime latency          = 3.0; /* Latency in seconds */

    for(auto &monitorItem : folderList) {
        // Create the event stream with out trampoline as callback
        monitorItem.eventStream = FSEventStreamCreate(
                nullptr,
                &fsNotifyTrampoline,
                &context, //NULL, // could put stream-specific data here. FSEventStreamRef stream;*/
                monitorItem.pathsToWatch,
                kFSEventStreamEventIdSinceNow, /* Or a previous event ID */
                latency,
                kFSEventStreamCreateFlagFileEvents /* Flags explained in reference: https://developer.apple.com/library/mac/documentation/Darwin/Reference/FSEvents_Ref/Reference/reference.html */
        );


        // Attach to the thread dispatch queue
        auto dispatchQ = dispatch_get_main_queue();
        if (monitorItem.exclusions.size() > 0) {
            FSEventStreamSetExclusionPaths(monitorItem.eventStream, monitorItem.pathsToExclude);
        }

        FSEventStreamSetDispatchQueue(monitorItem.eventStream, dispatchQ);
        if (!FSEventStreamStart( monitorItem.eventStream )) {
            auto logger = gnilk::Logger::GetLogger("FolderMonitor");
            logger->Error("Unable to start monitoring for path: ", monitorItem.pathName.c_str());
        }

    }
    isRunning = true;
    return true;
}

bool MacOSFolderMonitor::Stop() {
    // Already stopped??
    if (!isRunning) {
        return true;
    }
    std::lock_guard<std::mutex> guard(dataGuard);
    // Release and dispose of the event stream...
    for(auto &item : folderList) {
        FSEventStreamInvalidate(item.eventStream);
        FSEventStreamStop(item.eventStream);
        FSEventStreamRelease(item.eventStream);
    }

    isRunning = false;

    return true;
}

bool MacOSFolderMonitor::AddPath(const std::filesystem::path &path, const std::vector<std::string> &exclusions) {
    if (isRunning) {
        Stop();
    }
    std::lock_guard<std::mutex> guard(dataGuard);

    MonitorItem item;

    item.pathName = path;
    item.exclusions = exclusions;

    item.pathToWatch = CFStringCreateWithCString(nullptr, path.c_str(), kCFStringEncodingUTF8);
    //CFArrayRef pathsToWatch         = CFArrayCreate( NULL, ( const void ** ) &mypath, 1, NULL );
    item.pathsToWatch = CFArrayCreate(nullptr, (const void **) &item.pathToWatch, 1, nullptr);

    item.pathsToExclude = CFArrayCreateMutable(nullptr, exclusions.size(), nullptr);

    for(auto &s : exclusions) {
        auto exPath = path / s;
        CFStringRef pathToExclude = CFStringCreateWithCString(nullptr, exPath.c_str(), kCFStringEncodingUTF8);
        CFArrayAppendValue(item.pathsToExclude, pathToExclude);
    }
    item.exclusions = exclusions;
    folderList.push_back(item);

    return true;
}

void MacOSFolderMonitor::OnFSEvent(const std::string &path, kChangeFlags flags) {
    std::lock_guard<std::mutex> guard(dataGuard);

    auto logger = gnilk::Logger::GetLogger("FolderMonitor");
    logger->Debug("0x%x : %s", static_cast<int>(flags), path.c_str());
    if (flags & kChangeFlags::kCreated) {
        logger->Debug("Created!");
    }

    DispatchEvent(path, flags);
}


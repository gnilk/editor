//
// Created by gnilk on 29.08.23.
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


    /* Define variables and create a CFArray object containing CFString objects containing paths to watch. */
    CFStringRef mypath              = CFStringCreateWithCString( nullptr, folderList[0].c_str(), kCFStringEncodingUTF8);
    CFArrayRef pathsToWatch         = CFArrayCreate( nullptr, ( const void ** ) &mypath, 1, nullptr );
    CFAbsoluteTime latency          = 3.0; /* Latency in seconds */

    FSEventStreamContext context = {};
    context.info = this;


    /* Create the stream, passing in a callback */
    eventStream = FSEventStreamCreate(

            nullptr,
            &fsNotifyTrampoline,
            &context, //NULL, // could put stream-specific data here. FSEventStreamRef stream;*/
            pathsToWatch,
            kFSEventStreamEventIdSinceNow, /* Or a previous event ID */
            latency,
            kFSEventStreamCreateFlagFileEvents /* Flags explained in reference: https://developer.apple.com/library/mac/documentation/Darwin/Reference/FSEvents_Ref/Reference/reference.html */

    );

    // Attach to the thread dispatch queue
    auto dispatchQ = dispatch_get_main_queue();

    FSEventStreamSetDispatchQueue(eventStream, dispatchQ);
    if (!FSEventStreamStart( eventStream )) {
        auto logger = gnilk::Logger::GetLogger("FolderMonitor");
        logger->Error("Unable to start!");
        return false;
    }

    isRunning = true;
    return true;
}

bool MacOSFolderMonitor::Stop() {
    return true;
}

bool MacOSFolderMonitor::AddPath(const std::filesystem::path &path) {
    if (isRunning) {
        Stop();
    }
    std::lock_guard<std::mutex> guard(dataGuard);
    folderList.push_back(path);

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


//
// Created by gnilk on 29.08.23.
//
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <vector>
#include "logger.h"

#include "MacOSFolderMonitor.h"

using namespace gedit;

// FSEvents can only exclude this many paths - the rest is caught by the base's filter
static constexpr size_t kMaxOSExclusionPaths = 8;
// Seconds FSEvents collects before calling back - NoDefer delivers the first event of a quiet period at once
static constexpr CFAbsoluteTime kStreamLatency = 0.3;

static void glbFSNotifyTrampoline(

        ConstFSEventStreamRef streamRef,
        void *clientCallBackInfo,
        size_t numEvents,
        void *eventPaths,
        const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId eventIds[]

);


FolderMonitor::MonitorPoint::Ref MacOSFolderMonitor::CreateMonitorPoint(const std::filesystem::path &pathToMonitor) {
    return MacOSFolderMonitorPoint::Create(pathToMonitor);
}



MacOSFolderMonitorPoint::~MacOSFolderMonitorPoint() {
    Stop();
}

bool MacOSFolderMonitorPoint::Start() {
    if (isRunning) {
        return true;
    }
    auto logger = gnilk::Logger::GetLogger("FolderMonitor");

    auto cfPathToWatch = CFStringCreateWithCString(nullptr, pathToMonitor.c_str(), kCFStringEncodingUTF8);
    auto cfPathsToWatch = CFArrayCreate(nullptr, (const void **) &cfPathToWatch, 1, &kCFTypeArrayCallBacks);
    CFRelease(cfPathToWatch);

    // The callback gets 'this' - Stop drains the queue before we can go away
    fsContext = {};
    fsContext.info = this;

    // FileEvents: per-file paths (the folder to reconcile is the parent). WatchRoot: the root itself being
    // moved/deleted is reported (RootChanged).
    fsEventStream = FSEventStreamCreate(
            nullptr,
            &glbFSNotifyTrampoline,
            &fsContext,
            cfPathsToWatch,
            kFSEventStreamEventIdSinceNow,
            kStreamLatency,
            kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagWatchRoot
    );
    CFRelease(cfPathsToWatch);
    if (fsEventStream == nullptr) {
        logger->Error("Unable to create the event stream for: %s", pathToMonitor.c_str());
        return false;
    }

    SetOSExclusionPaths();

    dispatchQueue = dispatch_queue_create("goatedit.foldermonitor", DISPATCH_QUEUE_SERIAL);
    FSEventStreamSetDispatchQueue(fsEventStream, dispatchQueue);

    // Running before the stream starts - an event may arrive before FSEventStreamStart returns
    isRunning = true;
    if (!FSEventStreamStart(fsEventStream)) {
        logger->Error("Unable to start monitoring for path: %s", pathToMonitor.c_str());
        Stop();
        return false;
    }
    logger->Debug("Monitoring: %s", pathToMonitor.c_str());
    return true;
}

void MacOSFolderMonitorPoint::SetOSExclusionPaths() {
    auto &filter = GetExcludeFilter();
    if (filter.IsEmpty()) {
        return;
    }

    // Only folders that exist now at the top level - a nested or later match is still dropped by the filter
    std::vector<std::filesystem::path> toExclude;
    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator(pathToMonitor, ec); !ec && (it != std::filesystem::directory_iterator()); it.increment(ec)) {
        if (toExclude.size() == kMaxOSExclusionPaths) {
            break;
        }
        std::error_code ecDir;
        if (it->is_directory(ecDir) && filter.IsExcluded(it->path())) {
            toExclude.push_back(it->path());
        }
    }
    if (toExclude.empty()) {
        return;
    }

    auto cfPathsToExclude = CFArrayCreateMutable(nullptr, static_cast<CFIndex>(toExclude.size()), &kCFTypeArrayCallBacks);
    for (auto &path : toExclude) {
        auto cfPath = CFStringCreateWithCString(nullptr, path.c_str(), kCFStringEncodingUTF8);
        CFArrayAppendValue(cfPathsToExclude, cfPath);
        CFRelease(cfPath);
    }
    if (!FSEventStreamSetExclusionPaths(fsEventStream, cfPathsToExclude)) {
        auto logger = gnilk::Logger::GetLogger("FolderMonitor");
        logger->Warning("Unable to set the exclusion paths for: %s", pathToMonitor.c_str());
    }
    CFRelease(cfPathsToExclude);
}

// Main thread. Never called from the stream's queue (it would wait for itself).
bool MacOSFolderMonitorPoint::Stop() {
    // Events still in flight are dropped by the base from here on
    isRunning = false;
    if (fsEventStream != nullptr) {
        FSEventStreamStop(fsEventStream);
        FSEventStreamInvalidate(fsEventStream);
        FSEventStreamRelease(fsEventStream);
        fsEventStream = nullptr;
    }
    if (dispatchQueue != nullptr) {
        // No new callbacks after Invalidate - wait for one that is already running, it holds 'this'
        dispatch_sync_f(dispatchQueue, nullptr, [](void *) {});
        dispatch_release(dispatchQueue);
        dispatchQueue = nullptr;
    }
    return true;
}

void MacOSFolderMonitorPoint::OnFSEvents(size_t numEvents, const char **paths, const FSEventStreamEventFlags eventFlags[]) {
    // The other flags are historical and coalesced (a rename has no create/remove, a later event still says
    // 'created') - only the path is used, the folder is reconciled. These mean the reported paths are not
    // the whole story.
    static constexpr FSEventStreamEventFlags lostFlags = kFSEventStreamEventFlagMustScanSubDirs |
                                                         kFSEventStreamEventFlagUserDropped |
                                                         kFSEventStreamEventFlagKernelDropped |
                                                         kFSEventStreamEventFlagRootChanged;
    for (size_t i = 0; i < numEvents; i++) {
        if (eventFlags[i] & lostFlags) {
            OnLostEvents();
            continue;
        }
        OnRawEvent(paths[i]);
    }
}


//
// This is the C-callback from macOS when events happen, on the stream's dispatch queue
//
static void glbFSNotifyTrampoline(

        ConstFSEventStreamRef streamRef,
        void *clientCallBackInfo,
        size_t numEvents,
        void *eventPaths,
        const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId eventIds[]

){
    if (clientCallBackInfo == nullptr) {
        return;
    }
    auto pThis = static_cast<MacOSFolderMonitorPoint *>(clientCallBackInfo);
    pThis->OnFSEvents(numEvents, static_cast<const char **>(eventPaths), eventFlags);
}

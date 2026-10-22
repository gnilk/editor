//
// Created by gnilk on 29.08.23.
//
#include <CoreServices/CoreServices.h>
#include <mutex>
#include "logger.h"
#include "Core/Config/Config.h"
#include "Core/PathUtil.h"
#include "Core/StrUtil.h"

#include "MacOSFolderMonitor.h"

using namespace gedit;


static void glbFSNotifyTrampoline(

        ConstFSEventStreamRef streamRef,
        void *clientCallBackInfo,
        size_t numEvents,
        void *eventPaths,
        const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId eventIds[]

);


FolderMonitor::MonitorPoint::Ref MacOSFolderMonitor::CreateMonitorPoint(const std::filesystem::path &pathToMonitor) {
    auto monitor = MacOSFolderMonitorPoint::Create(pathToMonitor);
    monitor->SetExcludePaths(GetExclusionPaths());
    return monitor;
}



MacOSFolderMonitorPoint::~MacOSFolderMonitorPoint() {
    if (isRunning) {
        Stop();
    }
    if (cfPathToWatch) CFRelease(cfPathToWatch);
    if (cfPathsToWatch) CFRelease(cfPathsToWatch);
    if (cfPathsToExclude) CFRelease(cfPathsToExclude);
}

bool MacOSFolderMonitorPoint::Start() {
    if (isRunning) {
        return true;
    }

    std::lock_guard<std::mutex> guard(dataGuard);

    cfPathToWatch = CFStringCreateWithCString(nullptr, pathToMonitor.c_str(), kCFStringEncodingUTF8);
    cfPathsToWatch = CFArrayCreate(nullptr, (const void **) &cfPathToWatch, 1, nullptr);



    // Setup context so we can jump into the class
    fsContext.info = this;

    CFAbsoluteTime latency          = 3.0; /* Latency in seconds */

    // Create the event stream with out trampoline as callback
    // FIXME: consider passing 'kFSEventStreamCreateFlagNone' and check
    // perhaps test 'kFSEventStreamCreateFlagNoDefer' first
    // also, see documentation for: kFSEventStreamCreateFlagWatchRoot = 0x00000004
    // add this: kFSEventStreamCreateFlagIgnoreSelf
    //

    fsEventStream = FSEventStreamCreate(
            nullptr,
            &glbFSNotifyTrampoline,
            &fsContext, //NULL, // could put stream-specific data here. FSEventStreamRef stream;*/
            cfPathsToWatch,
            kFSEventStreamEventIdSinceNow, /* Or a previous event ID */
            latency,
            kFSEventStreamCreateFlagFileEvents /* Flags explained in reference: https://developer.apple.com/library/mac/documentation/Darwin/Reference/FSEvents_Ref/Reference/reference.html */
    );


    // Get the main dispatch queue - we will attach to this one..
    auto dispatchQ = dispatch_get_main_queue();

    // FIXME: Split exclusion paths in OS supported and what we need to support..
    //        MacOS can only handle 8 paths in the exclude list, the rest we need to filter in user-land...
    // Have exclusion paths? - let's incorporate them...
    if (!pathsToExclude.empty()) {
        cfPathsToExclude = CFArrayCreateMutable(nullptr, pathsToExclude.size(), nullptr);
        for (auto &s: pathsToExclude) {
            auto exPath = std::filesystem::path(pathToMonitor) / s;
            CFStringRef cfPathToExclude = CFStringCreateWithCString(nullptr, exPath.c_str(), kCFStringEncodingUTF8);
            CFArrayAppendValue(cfPathsToExclude, cfPathToExclude);
        }
        FSEventStreamSetExclusionPaths(fsEventStream, cfPathsToExclude);
    }

    // Attach to dispatch queue and kick this off
    FSEventStreamSetDispatchQueue(fsEventStream, dispatchQ);
    if (!FSEventStreamStart( fsEventStream )) {
        auto logger = gnilk::Logger::GetLogger("MacOSFolderMonitor");
        logger->Error("Unable to start monitoring for path: ", pathToMonitor.c_str());
    }

    isRunning = true;
    return true;
}

bool MacOSFolderMonitorPoint::Stop() {
    // Already stopped??
    if (!isRunning) {
        return true;
    }
    std::lock_guard<std::mutex> guard(dataGuard);
    // Release and dispose of the event stream...
    FSEventStreamStop(fsEventStream);
    FSEventStreamInvalidate(fsEventStream);
    FSEventStreamRelease(fsEventStream);


    isRunning = false;
    return true;
}
void MacOSFolderMonitorPoint::OnFSEvent(const std::string &path) {
    OnRawEvent(path);
}




//
// This is the C-callback from macOS when events happen
// We basically just translates the stuff and jump to our callback
//
static void glbFSNotifyTrampoline(

        ConstFSEventStreamRef streamRef,
        void *clientCallBackInfo,
        size_t numEvents,
        void *eventPaths,
        const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId eventIds[]

){

    if (clientCallBackInfo != nullptr) {
        auto pThis = static_cast<MacOSFolderMonitorPoint *>(clientCallBackInfo);
        const char **paths = (const char **)eventPaths;

        for(auto i=0;i<numEvents;i++) {
            // The flags are historical and coalesced - not trusted, the folder is reconciled instead
            std::string path(paths[i]);
            pThis->OnFSEvent(path);
        }
    }

}


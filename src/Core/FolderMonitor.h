//
// Created by gnilk on 29.08.23.
//

#ifndef GOATEDIT_FOLDERMONITOR_H
#define GOATEDIT_FOLDERMONITOR_H

#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <atomic>
#include <memory>
#include <functional>
#include <filesystem>

#include "Core/FileSystem/FsFilter.h"

namespace gedit {

    //
    // Monitors a directory tree and reports which FOLDERS may differ from what was last read - a dirty-
    // directory signal, not a change description (see docs/foldermonitor_fixes.md §2). The consumer
    // re-reads (reconciles) those folders. Renames, moves, atomic saves and coalesced OS events are then
    // all the same case.
    //
    // 1) Create a point for the tree root:  auto monitor = folderMonitor.CreateMonitorPoint("/some/path");
    // 2) Set the exclude filter, the main-thread poster and the dirty handler
    // 3) monitor->Start();
    //
    // The OS backend calls OnRawEvent / OnLostEvents from its own thread. The base canonicalizes and
    // filters the path, collects the dirty folders, and posts ONE flush to the main thread until that
    // flush has run - a burst of events becomes one call to the dirty handler.
    //
    class FolderMonitor {
    public:
        class MonitorPoint : public std::enable_shared_from_this<MonitorPoint> {
        public:
            using Ref = std::shared_ptr<MonitorPoint>;
            // Runs a closure on the main thread - injected, the monitor knows nothing about the runloop/UI
            using PostDelegate = std::function<void(std::function<void()>)>;
            // Main thread: the folders that may differ from the tree since the last call. 'isLostEvents' when
            // the OS dropped events - everything loaded below the root may be stale.
            using DirtyDelegate = std::function<void(const std::set<std::filesystem::path> &dirtyFolders, bool isLostEvents)>;
        public:
            explicit MonitorPoint(const std::filesystem::path &path);
            virtual ~MonitorPoint() = default;

            static Ref Create(const std::filesystem::path &pathToMonitor) {
                return std::make_shared<MonitorPoint>(pathToMonitor);
            }

            virtual bool Start() {
                return false;
            }
            virtual bool Stop() {
                return false;
            }

            bool IsRunning() {
                return isRunning;
            }

            const std::filesystem::path &GetRootPath() const {
                return pathToMonitor;
            }

            // Set these before Start - the OS thread reads the filter without a lock
            void SetExcludeFilter(const FsFilter &newExcludeFilter);
            void SetPostToMain(PostDelegate newPostToMain);
            void SetDirtyHandler(DirtyDelegate newDirtyHandler);

            // True for a path the tree never shows: outside the root, or below an excluded folder (any
            // component - 'cmake-build-debug/x/y.o'). Takes canonical paths.
            bool IsPathExcluded(const std::filesystem::path &path) const;

        protected:
            MonitorPoint() = default;
            // For OS-level exclusion (an optimisation only - OnRawEvent applies the filter regardless)
            const FsFilter &GetExcludeFilter() const {
                return excludeFilter;
            }
            // Any thread: something happened at 'path' (a file or a folder)
            void OnRawEvent(const std::filesystem::path &path);
            // Any thread: the OS dropped events
            void OnLostEvents();
        private:
            // Caller holds dataGuard
            void PostFlush();
            // Main thread
            void Flush();
        protected:
            std::atomic_bool isRunning = false;
            std::mutex dataGuard;
            std::filesystem::path pathToMonitor = {};
        private:
            FsFilter excludeFilter = {};
            PostDelegate postToMain = {};
            DirtyDelegate dirtyHandler = {};
            std::set<std::filesystem::path> dirtyFolders = {};
            bool isLostEvents = false;
            bool isFlushPending = false;
        };

    public:
        FolderMonitor() = default;
        virtual ~FolderMonitor() = default;

        // The caller owns the point; the base implementation never starts (no OS backend)
        virtual MonitorPoint::Ref CreateMonitorPoint(const std::filesystem::path &pathToMonitor);
        // config 'foldermonitor.enabled'
        bool IsEnabled();
    protected:
        const std::vector<std::string> &GetExclusionPaths();
    private:
        std::vector<std::string> exclusionPaths;
    };
}

#endif //GOATEDIT_FOLDERMONITOR_H

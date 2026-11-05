//
// Created by gnilk on 29.08.23.
//
#include "FolderMonitor.h"
#include "PathUtil.h"
#include "Core/Config/Config.h"

static std::string cfgSectionName = "foldermonitor";

using namespace gedit;

FolderMonitor::MonitorPoint::Ref FolderMonitor::CreateMonitorPoint(const std::filesystem::path &pathToMonitor) {
    return MonitorPoint::Create(pathToMonitor);
}

// Canonical, like the workspace tree - the OS reports real paths
FolderMonitor::MonitorPoint::MonitorPoint(const std::filesystem::path &path) : pathToMonitor(pathutil::CanonicalPath(path)) {

}

void FolderMonitor::MonitorPoint::SetExclusionRules(ExclusionRules::Ref newRules) {
    std::lock_guard<std::mutex> guard(dataGuard);
    exclusionRules = std::move(newRules);
}

void FolderMonitor::MonitorPoint::SetPostToMain(PostDelegate newPostToMain) {
    std::lock_guard<std::mutex> guard(dataGuard);
    postToMain = std::move(newPostToMain);
}

void FolderMonitor::MonitorPoint::SetDirtyHandler(DirtyDelegate newDirtyHandler) {
    std::lock_guard<std::mutex> guard(dataGuard);
    dirtyHandler = std::move(newDirtyHandler);
}

bool FolderMonitor::MonitorPoint::IsPathExcluded(const std::filesystem::path &path) const {
    auto relPath = path.lexically_normal().lexically_relative(pathToMonitor);
    if (relPath.empty() || (*relPath.begin() == "..")) {
        return true;
    }
    // The root itself
    if (relPath == ".") {
        return false;
    }
    if (exclusionRules == nullptr) {
        return false;
    }
    std::error_code error;
    return !exclusionRules->IsIncluded(path, std::filesystem::is_directory(path, error));
}

// protected - called from the OS thread
void FolderMonitor::MonitorPoint::OnRawEvent(const std::filesystem::path &path) {
    if (!isRunning) {
        return;
    }
    // A removed entry can't be canonicalized, its folder usually can (CanonicalPath keeps the missing tail)
    auto canonical = pathutil::CanonicalPath(path);
    if (IsPathExcluded(canonical)) {
        return;
    }

    std::lock_guard<std::mutex> guard(dataGuard);
    if (canonical == pathToMonitor) {
        dirtyFolders.insert(canonical);
    } else {
        // The entry's folder lists it (added / gone / renamed); a folder event may also mean its contents changed
        dirtyFolders.insert(canonical.parent_path());
        std::error_code ec;
        if (std::filesystem::is_directory(canonical, ec)) {
            dirtyFolders.insert(canonical);
        }
    }
    PostFlush();
}

void FolderMonitor::MonitorPoint::OnLostEvents() {
    if (!isRunning) {
        return;
    }
    std::lock_guard<std::mutex> guard(dataGuard);
    isLostEvents = true;
    PostFlush();
}

// One flush in flight at a time - everything arriving before it runs is folded into it
void FolderMonitor::MonitorPoint::PostFlush() {
    if (isFlushPending || (postToMain == nullptr)) {
        return;
    }
    isFlushPending = true;
    // The point may be gone (its ProjectRoot closed) before the main thread gets to this
    std::weak_ptr<MonitorPoint> weakThis = weak_from_this();
    postToMain([weakThis]() {
        if (auto self = weakThis.lock()) {
            self->Flush();
        }
    });
}

void FolderMonitor::MonitorPoint::Flush() {
    std::set<std::filesystem::path> folders;
    bool isLost = false;
    DirtyDelegate handler;
    {
        std::lock_guard<std::mutex> guard(dataGuard);
        folders.swap(dirtyFolders);
        isLost = isLostEvents;
        isLostEvents = false;
        isFlushPending = false;
        handler = dirtyHandler;
    }
    if (handler != nullptr) {
        handler(folders, isLost);
    }
}

// Support functions
bool FolderMonitor::IsEnabled() {
    if (!Config::Instance()[cfgSectionName].GetBool("enabled", true)) {
        return false;
    }
    return true;
}

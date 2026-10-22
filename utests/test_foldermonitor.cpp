//
// Folder monitor - the OS backend against a real filesystem. The base (filter, coalescing, flush) is covered
// OS-independently in test_workspace with a fake monitor point; this only checks that the platform stream
// delivers. macOS only for now (FSEvents); the Linux backend gets its cases with step 7.
//
#include <testinterface.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <set>
#include <thread>
#include <vector>
#include "Core/FolderMonitor.h"
#include "Core/PathUtil.h"
#ifdef GEDIT_MACOS
#include "Core/macOS/MacOSFolderMonitor.h"
#endif

using namespace gedit;
using namespace std::chrono_literals;

extern "C" {
DLL_EXPORT int test_foldermonitor(ITesting *t);
DLL_EXPORT int test_foldermonitor_fsevents(ITesting *t);
DLL_EXPORT int test_foldermonitor_stopinflight(ITesting *t);
}

DLL_EXPORT int test_foldermonitor(ITesting *t) {
    return kTR_Pass;
}

#ifdef GEDIT_MACOS

// Posts arrive on the stream's queue; the test thread plays the main thread
struct PostCollector {
    std::mutex lock;
    std::vector<std::function<void()>> posted = {};

    void Post(std::function<void()> callback) {
        std::lock_guard<std::mutex> guard(lock);
        posted.push_back(std::move(callback));
    }
    // Wait for at least one post, then run what's there. False on timeout.
    bool WaitAndRun(std::chrono::milliseconds timeout) {
        auto tEnd = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < tEnd) {
            std::vector<std::function<void()>> toRun;
            {
                std::lock_guard<std::mutex> guard(lock);
                toRun.swap(posted);
            }
            if (!toRun.empty()) {
                for (auto &callback : toRun) {
                    callback();
                }
                return true;
            }
            std::this_thread::sleep_for(20ms);
        }
        return false;
    }
};

DLL_EXPORT int test_foldermonitor_fsevents(ITesting *t) {
    namespace fs = std::filesystem;
    // Under /var/folders -> /private/var/...: the stream reports real paths, the point is canonical
    auto root = pathutil::CanonicalPath(fs::temp_directory_path() / "goat_fm_fsevents");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "src");
    fs::create_directories(root / "cmake-build-debug");     // exists at Start -> also excluded at the OS level

    PostCollector collector;
    std::set<fs::path> dirty;
    bool isLost = false;

    auto monitor = MacOSFolderMonitorPoint::Create(fs::temp_directory_path() / "goat_fm_fsevents");
    monitor->SetExcludeFilter(FsFilter({"cmake-build-*"}));
    monitor->SetPostToMain([&collector](std::function<void()> callback) { collector.Post(std::move(callback)); });
    monitor->SetDirtyHandler([&dirty, &isLost](const std::set<fs::path> &folders, bool isLostEvents) {
        dirty.insert(folders.begin(), folders.end());
        isLost |= isLostEvents;
    });
    TR_ASSERT(t, monitor->Start());
    TR_ASSERT(t, monitor->IsRunning());

    { std::ofstream(root / "src" / "a.txt") << "x\n"; }
    TR_ASSERT(t, collector.WaitAndRun(3000ms));
    TR_ASSERT(t, dirty.contains(root / "src"));
    TR_ASSERT(t, !isLost);

    // A rename: both names' folder is dirty, no matter what the flags claim
    dirty.clear();
    fs::rename(root / "src" / "a.txt", root / "b.txt");
    TR_ASSERT(t, collector.WaitAndRun(3000ms));
    TR_ASSERT(t, dirty.contains(root / "src"));
    TR_ASSERT(t, dirty.contains(root));

    // A build: nothing reaches the main thread (latency 0.3 s - a second is plenty)
    dirty.clear();
    fs::create_directories(root / "cmake-build-debug" / "CMakeFiles");
    { std::ofstream(root / "cmake-build-debug" / "CMakeFiles" / "y.o") << "x\n"; }
    fs::create_directories(root / "cmake-build-release");                 // created later: the filter's job
    { std::ofstream(root / "cmake-build-release" / "z.o") << "x\n"; }
    TR_ASSERT(t, !collector.WaitAndRun(1000ms));
    TR_ASSERT(t, dirty.empty());

    TR_ASSERT(t, monitor->Stop());
    TR_ASSERT(t, !monitor->IsRunning());
    // Stopped: nothing more arrives
    { std::ofstream(root / "c.txt") << "x\n"; }
    TR_ASSERT(t, !collector.WaitAndRun(1000ms));

    fs::remove_all(root, ec);
    return kTR_Pass;
}

// Stop/destroy while the stream is busy - the callback holds a raw 'this', Stop must drain it
DLL_EXPORT int test_foldermonitor_stopinflight(ITesting *t) {
    namespace fs = std::filesystem;
    auto root = pathutil::CanonicalPath(fs::temp_directory_path() / "goat_fm_inflight");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);

    PostCollector collector;
    for (int round = 0; round < 5; round++) {
        auto monitor = MacOSFolderMonitorPoint::Create(root);
        monitor->SetPostToMain([&collector](std::function<void()> callback) { collector.Post(std::move(callback)); });
        TR_ASSERT(t, monitor->Start());
        for (int i = 0; i < 200; i++) {
            std::ofstream(root / ("f" + std::to_string(i) + ".txt")) << round << "\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(round * 100));
        // destroyed here - the destructor stops it
    }
    // Flushes queued for dead points do nothing
    collector.WaitAndRun(200ms);

    fs::remove_all(root, ec);
    return kTR_Pass;
}

#else

DLL_EXPORT int test_foldermonitor_fsevents(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_foldermonitor_stopinflight(ITesting *t) {
    return kTR_Pass;
}

#endif

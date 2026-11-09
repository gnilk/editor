//
// FileFinder (docs/usability-round.md §3 b) - every file of a root, what its rules leave out left out, walked in the
// background into a snapshot
//
#include <testinterface.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "Core/FileSystem/ExclusionRules.h"
#include "Core/FileSystem/FileFinder.h"

using namespace gedit;
namespace fs = std::filesystem;

extern "C" {
    DLL_EXPORT int test_filefinder(ITesting *t);
    DLL_EXPORT int test_filefinder_walk(ITesting *t);
    DLL_EXPORT int test_filefinder_rules(ITesting *t);
    DLL_EXPORT int test_filefinder_symlink(ITesting *t);
    DLL_EXPORT int test_filefinder_depth(ITesting *t);
    DLL_EXPORT int test_filefinder_snapshot(ITesting *t);
    DLL_EXPORT int test_filefinder_truncated(ITesting *t);
}

namespace {
    fs::path MakeRoot(const std::string &name) {
        auto root = fs::canonical(fs::temp_directory_path()) / ("goatedit_filefinder_" + name);
        fs::remove_all(root);
        fs::create_directories(root);
        return root;
    }

    void WriteFile(const fs::path &path, const std::string &text = "x") {
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file << text;
    }

    // The files a walk finds, relative to the root, sorted
    std::vector<std::string> WalkRelative(const FileFinder::Root &root, FileFinder::Options options = {}) {
        std::vector<std::string> found;
        std::atomic<bool> isStopping = false;
        FileFinder::Walk(root, options, [&](const fs::path &path) {
            found.push_back(path.lexically_relative(root.path).generic_string());
            return true;
        }, isStopping);
        std::sort(found.begin(), found.end());
        return found;
    }

    // Wait for 'count' walks to be done
    struct DoneWaiter {
        std::mutex lock;
        std::condition_variable done;
        int numDone = 0;

        FileFinder::DoneHandler Handler() {
            return [this]() {
                std::lock_guard guard(lock);
                numDone++;
                done.notify_all();
            };
        }
        bool WaitFor(int count) {
            std::unique_lock guard(lock);
            return done.wait_for(guard, std::chrono::seconds(10), [this, count]() { return numDone >= count; });
        }
    };
}

DLL_EXPORT int test_filefinder(ITesting *t) {
    return kTR_Pass;
}

// Every file, in every folder
DLL_EXPORT int test_filefinder_walk(ITesting *t) {
    auto root = MakeRoot("walk");
    WriteFile(root / "main.cpp");
    WriteFile(root / "src/Core/Document.cpp");
    WriteFile(root / "src/Core/Document.h");
    WriteFile(root / "docs/readme.md");
    fs::create_directories(root / "empty");
    auto found = WalkRelative({root, nullptr});
    TR_ASSERT(t, found == std::vector<std::string>({"docs/readme.md", "main.cpp", "src/Core/Document.cpp", "src/Core/Document.h"}));
    fs::remove_all(root);
    return kTR_Pass;
}

// What the rules leave out isn't listed - an ignored folder isn't entered, an excluded one neither
DLL_EXPORT int test_filefinder_rules(ITesting *t) {
    auto root = MakeRoot("rules");
    WriteFile(root / ".gitignore", "build/\n*.o\n");
    WriteFile(root / "main.cpp");
    WriteFile(root / "main.o");
    WriteFile(root / "build/out.txt");
    WriteFile(root / ".git/HEAD");
    auto rules = ExclusionRules::Create(root, {".git"}, true);
    auto found = WalkRelative({root, rules});
    TR_ASSERT(t, found == std::vector<std::string>({".gitignore", "main.cpp"}));
    fs::remove_all(root);
    return kTR_Pass;
}

// A link to a folder isn't followed (a link to its own parent would never end); a link to a file is a file
DLL_EXPORT int test_filefinder_symlink(ITesting *t) {
    auto root = MakeRoot("symlink");
    WriteFile(root / "a/file.txt");
    std::error_code error;
    fs::create_directory_symlink(root, root / "a/loop", error);
    fs::create_symlink(root / "a/file.txt", root / "link.txt", error);
    auto found = WalkRelative({root, nullptr});
    TR_ASSERT(t, found == std::vector<std::string>({"a/file.txt", "link.txt"}));
    fs::remove_all(root);
    return kTR_Pass;
}

DLL_EXPORT int test_filefinder_depth(ITesting *t) {
    auto root = MakeRoot("depth");
    WriteFile(root / "1.txt");
    WriteFile(root / "a/2.txt");
    WriteFile(root / "a/b/3.txt");
    FileFinder::Options options;
    options.maxDepth = 2;
    TR_ASSERT(t, WalkRelative({root, nullptr}, options) == std::vector<std::string>({"1.txt", "a/2.txt"}));
    fs::remove_all(root);
    return kTR_Pass;
}

// A refresh walks in the background and swaps in a whole new list; each walk is a new generation
DLL_EXPORT int test_filefinder_snapshot(ITesting *t) {
    auto root = MakeRoot("snapshot");
    WriteFile(root / "one.txt");
    auto finder = FileFinder::Create();
    TR_ASSERT(t, finder->GetSnapshot()->generation == 0);
    TR_ASSERT(t, finder->GetSnapshot()->files.empty());
    DoneWaiter waiter;
    finder->SetDoneHandler(waiter.Handler());

    finder->Refresh({{root, nullptr}});
    TR_ASSERT(t, waiter.WaitFor(1));
    auto first = finder->GetSnapshot();
    TR_ASSERT(t, first->generation == 1);
    TR_ASSERT(t, first->files.size() == 1);
    TR_ASSERT(t, first->files[0].path == (root / "one.txt").string());
    TR_ASSERT(t, first->files[0].idxRoot == 0);

    WriteFile(root / "two.txt");
    finder->Refresh({{root, nullptr}});
    TR_ASSERT(t, waiter.WaitFor(2));
    auto second = finder->GetSnapshot();
    TR_ASSERT(t, second->generation == 2);
    TR_ASSERT(t, second->files.size() == 2);
    // The first list is untouched - a reader holding it keeps a whole list
    TR_ASSERT(t, first->files.size() == 1);
    fs::remove_all(root);
    return kTR_Pass;
}

// A root bigger than the limit: the walk stops there, and says so
DLL_EXPORT int test_filefinder_truncated(ITesting *t) {
    auto root = MakeRoot("truncated");
    for (int i = 0; i < 10; i++) {
        WriteFile(root / ("f" + std::to_string(i) + ".txt"));
    }
    FileFinder::Options options;
    options.maxFiles = 4;
    auto finder = FileFinder::Create(options);
    DoneWaiter waiter;
    finder->SetDoneHandler(waiter.Handler());
    finder->Refresh({{root, nullptr}});
    TR_ASSERT(t, waiter.WaitFor(1));
    TR_ASSERT(t, finder->GetSnapshot()->files.size() == 4);
    TR_ASSERT(t, finder->GetSnapshot()->isTruncated);
    fs::remove_all(root);
    return kTR_Pass;
}

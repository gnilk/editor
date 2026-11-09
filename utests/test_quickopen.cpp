//
// QuickOpenModal (docs/usability-round.md §3 b) - every file of the roots, ranked by name; on a snapshot made by hand
//
#include <testinterface.h>

#include <string>
#include <unordered_set>
#include <vector>

#include "Core/Editor/Modals/QuickOpenModal.h"
#include "Core/FileSystem/FileFinder.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_quickopen(ITesting *t);
    DLL_EXPORT int test_quickopen_entries(ITesting *t);
    DLL_EXPORT int test_quickopen_rank(ITesting *t);
    DLL_EXPORT int test_quickopen_ties(ITesting *t);
    DLL_EXPORT int test_quickopen_path(ITesting *t);
    DLL_EXPORT int test_quickopen_empty(ITesting *t);
    DLL_EXPORT int test_quickopen_limit(ITesting *t);
}

namespace {
    FileFinder::Snapshot SnapshotOf(const std::vector<std::string> &relative) {
        FileFinder::Snapshot snapshot;
        snapshot.roots.push_back({"/proj", nullptr});
        for (auto &path : relative) {
            snapshot.files.push_back({"/proj/" + path, 0});
        }
        return snapshot;
    }

    const std::vector<std::string> kFiles = {
        "main.cpp",
        "src/Core/Document.cpp",
        "src/Core/Document.h",
        "src/Core/DocumentViewState.cpp",
        "src/Core/Editor/Views/EditorView.cpp",
        "src/Core/Editor.cpp",
        "docs/usability-round.md",
        "utests/test_document.cpp",
    };

    // The rows' paths below the root, in order
    std::vector<std::string> Ranked(const std::vector<QuickOpenModal::Entry> &entries, const std::u32string &query,
                                    size_t maxRows = QuickOpenModal::kMaxRows) {
        std::vector<std::string> out;
        for (auto idx : QuickOpenModal::Filter(entries, query, maxRows)) {
            out.push_back(entries[idx].path.substr(6));
        }
        return out;
    }
}

DLL_EXPORT int test_quickopen(ITesting *t) {
    return kTR_Pass;
}

// A file's name, its place below the root, whether it is open or beside the active one
DLL_EXPORT int test_quickopen_entries(ITesting *t) {
    auto entries = QuickOpenModal::EntriesOf(SnapshotOf(kFiles), "/proj/src/Core/Editor.cpp", {"/proj/main.cpp"});
    TR_ASSERT(t, entries.size() == kFiles.size());
    auto &document = entries[1];
    TR_ASSERT(t, document.name == U"Document.cpp");
    TR_ASSERT(t, document.lowerName == U"document.cpp");
    TR_ASSERT(t, document.relative == U"src/Core/Document.cpp");
    TR_ASSERT(t, document.folder == U"src/Core");
    TR_ASSERT(t, document.isNearActive);
    TR_ASSERT(t, !document.isOpen);
    TR_ASSERT(t, entries[0].folder.empty());
    TR_ASSERT(t, entries[0].isOpen);
    TR_ASSERT(t, !entries[0].isNearActive);
    TR_ASSERT(t, !entries[4].isNearActive);     // a folder below the active one isn't its folder
    return kTR_Pass;
}

// By name, as Go to symbol: a prefix before a word start before a subsequence
DLL_EXPORT int test_quickopen_rank(ITesting *t) {
    auto entries = QuickOpenModal::EntriesOf(SnapshotOf(kFiles), "", {});
    auto docs = Ranked(entries, U"doc");
    // The three names starting with it first, test_document.cpp (a word start) after them
    TR_ASSERT(t, docs.size() >= 4);
    TR_ASSERT(t, docs[3] == "utests/test_document.cpp");
    auto ev = Ranked(entries, U"edview");
    TR_ASSERT(t, !ev.empty() && (ev[0] == "src/Core/Editor/Views/EditorView.cpp"));
    // Case doesn't count
    TR_ASSERT(t, Ranked(entries, U"MAIN") == std::vector<std::string>({"main.cpp"}));
    TR_ASSERT(t, Ranked(entries, U"zzz").empty());
    return kTR_Pass;
}

// Among equal matches: the open files, then the active file's folder, then the shorter path
DLL_EXPORT int test_quickopen_ties(ITesting *t) {
    auto plain = QuickOpenModal::EntriesOf(SnapshotOf(kFiles), "", {});
    TR_ASSERT(t, (Ranked(plain, U"doc")[0] == "src/Core/Document.h"));      // shortest

    auto open = QuickOpenModal::EntriesOf(SnapshotOf(kFiles), "", {"/proj/src/Core/DocumentViewState.cpp"});
    TR_ASSERT(t, (Ranked(open, U"doc")[0] == "src/Core/DocumentViewState.cpp"));

    // The active file in 'utests': its folder first among the word-start matches
    auto near = QuickOpenModal::EntriesOf(SnapshotOf({"a/test_x.cpp", "utests/test_y.cpp", "b/test_z.cpp"}), "/proj/utests/x.cpp", {});
    TR_ASSERT(t, (Ranked(near, U"test")[0] == "utests/test_y.cpp"));
    return kTR_Pass;
}

// A '/' in the query: matched against the path below the root
DLL_EXPORT int test_quickopen_path(ITesting *t) {
    auto entries = QuickOpenModal::EntriesOf(SnapshotOf(kFiles), "", {});
    auto views = Ranked(entries, U"editor/view");
    TR_ASSERT(t, !views.empty() && (views[0] == "src/Core/Editor/Views/EditorView.cpp"));
    auto utests = Ranked(entries, U"utests/");
    TR_ASSERT(t, utests == std::vector<std::string>({"utests/test_document.cpp"}));
    return kTR_Pass;
}

// Nothing typed: every file, the open ones first
DLL_EXPORT int test_quickopen_empty(ITesting *t) {
    auto entries = QuickOpenModal::EntriesOf(SnapshotOf(kFiles), "", {"/proj/docs/usability-round.md"});
    auto all = Ranked(entries, U"");
    TR_ASSERT(t, all.size() == kFiles.size());
    TR_ASSERT(t, all[0] == "docs/usability-round.md");
    return kTR_Pass;
}

// No more rows than asked for - the best of them
DLL_EXPORT int test_quickopen_limit(ITesting *t) {
    std::vector<std::string> many;
    for (int i = 0; i < 1000; i++) {
        many.push_back("f/file" + std::to_string(i) + ".txt");
    }
    many.push_back("file.txt");
    auto entries = QuickOpenModal::EntriesOf(SnapshotOf(many), "", {});
    auto ranked = Ranked(entries, U"file", 10);
    TR_ASSERT(t, ranked.size() == 10);
    TR_ASSERT(t, ranked[0] == "file.txt");
    return kTR_Pass;
}

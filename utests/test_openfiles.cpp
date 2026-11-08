//
// The open files (docs/usability-round.md §1): which ones the header shows when they don't all fit, and the list's
// filter and folders - the parts that need no screen.
//
#include <testinterface.h>

#include <filesystem>
#include <string>
#include <vector>

#include "Core/Editor/Modals/OpenFilesModal.h"
#include "Core/Editor/Views/EditorHeaderView.h"

using namespace gedit;
namespace fs = std::filesystem;
using Span = EditorHeaderView::Span;

extern "C" {
    DLL_EXPORT int test_openfiles(ITesting *t);
    DLL_EXPORT int test_openfiles_layout_fits(ITesting *t);
    DLL_EXPORT int test_openfiles_layout_cut(ITesting *t);
    DLL_EXPORT int test_openfiles_layout_active(ITesting *t);
    DLL_EXPORT int test_openfiles_filter(ITesting *t);
    DLL_EXPORT int test_openfiles_folder(ITesting *t);
}

namespace {
    // What the header draws for a span: the entries, ' | ' between, '...' and ' | ' for each cut end
    int DrawnWidth(const std::vector<int> &widths, const Span &span) {
        int total = 0;
        for (size_t i = span.idxFirst; i < span.idxEnd; i++) {
            total += widths[i] + ((i > span.idxFirst) ? 3 : 0);
        }
        total += span.isCutBefore ? 6 : 0;
        total += span.isCutAfter ? 6 : 0;
        return total;
    }

    std::vector<OpenFilesModal::Entry> Entries(std::initializer_list<std::u32string> names) {
        std::vector<OpenFilesModal::Entry> entries;
        for (auto &name : names) {
            OpenFilesModal::Entry entry;
            entry.name = name;
            entries.push_back(entry);
        }
        return entries;
    }
}

DLL_EXPORT int test_openfiles(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_openfiles_layout_fits(ITesting *t) {
    auto all = EditorHeaderView::LayoutEntries({5, 5, 5}, 1, 40);
    TR_ASSERT(t, (all.idxFirst == 0) && (all.idxEnd == 3));
    TR_ASSERT(t, !all.isCutBefore && !all.isCutAfter);
    // Exactly: 5 + 3 + 5 + 3 + 5
    auto exact = EditorHeaderView::LayoutEntries({5, 5, 5}, 0, 21);
    TR_ASSERT(t, (exact.idxEnd == 3) && !exact.isCutAfter);
    TR_ASSERT(t, EditorHeaderView::LayoutEntries({}, 0, 40).idxEnd == 0);
    return kTR_Pass;
}

DLL_EXPORT int test_openfiles_layout_cut(ITesting *t) {
    std::vector<int> widths = {5, 5, 5, 5, 5, 5};
    // The first active: the end is cut
    auto start = EditorHeaderView::LayoutEntries(widths, 0, 20);
    TR_ASSERT(t, (start.idxFirst == 0) && (start.idxEnd == 2));
    TR_ASSERT(t, !start.isCutBefore && start.isCutAfter);
    // The last active: the row starts later, the start is cut
    auto end = EditorHeaderView::LayoutEntries(widths, 5, 20);
    TR_ASSERT(t, (end.idxFirst == 4) && (end.idxEnd == 6));
    TR_ASSERT(t, end.isCutBefore && !end.isCutAfter);
    // One in the middle: both
    auto middle = EditorHeaderView::LayoutEntries(widths, 3, 25);
    TR_ASSERT(t, (middle.idxFirst == 2) && (middle.idxEnd == 4));
    TR_ASSERT(t, middle.isCutBefore && middle.isCutAfter);
    // Wider than the row on its own: still the one shown
    auto wide = EditorHeaderView::LayoutEntries({50, 5}, 0, 10);
    TR_ASSERT(t, (wide.idxFirst == 0) && (wide.idxEnd == 1) && wide.isCutAfter);
    return kTR_Pass;
}

DLL_EXPORT int test_openfiles_layout_active(ITesting *t) {
    // Whatever the widths, the row and the active file: the active one is shown, and what is drawn fits - unless the
    // active one alone doesn't
    for (size_t numEntries = 1; numEntries <= 8; numEntries++) {
        std::vector<int> widths;
        for (size_t i = 0; i < numEntries; i++) {
            widths.push_back(3 + (int)((i * 7) % 11));
        }
        for (size_t idxActive = 0; idxActive < numEntries; idxActive++) {
            for (int width = 8; width <= 70; width++) {
                auto span = EditorHeaderView::LayoutEntries(widths, idxActive, width);
                TR_ASSERT(t, (span.idxFirst <= idxActive) && (idxActive < span.idxEnd));
                TR_ASSERT(t, span.isCutBefore == (span.idxFirst > 0));
                TR_ASSERT(t, span.isCutAfter == (span.idxEnd < numEntries));
                TR_ASSERT(t, (DrawnWidth(widths, span) <= width) || (span.idxEnd - span.idxFirst == 1));
            }
        }
    }
    return kTR_Pass;
}

DLL_EXPORT int test_openfiles_filter(ITesting *t) {
    auto entries = Entries({U"Workspace.cpp", U"Workspace.h", U"Editor.cpp", U"EditorView.cpp"});
    TR_ASSERT(t, (OpenFilesModal::Filter(entries, U"") == std::vector<size_t>{0, 1, 2, 3}));
    // Ties keep the header's order
    TR_ASSERT(t, (OpenFilesModal::Filter(entries, U"work") == std::vector<size_t>{0, 1}));
    // Case doesn't matter; a prefix ranks before a subsequence
    TR_ASSERT(t, (OpenFilesModal::Filter(entries, U"edit") == std::vector<size_t>{2, 3}));
    auto subsequence = OpenFilesModal::Filter(entries, U"wsh");
    TR_ASSERT(t, (subsequence.size() == 1) && (subsequence[0] == 1));
    TR_ASSERT(t, OpenFilesModal::Filter(entries, U"xyz").empty());
    return kTR_Pass;
}

DLL_EXPORT int test_openfiles_folder(ITesting *t) {
    auto root = fs::canonical(fs::temp_directory_path()) / "goatedit_openfiles";
    std::vector<fs::path> roots = {root};
    TR_ASSERT(t, OpenFilesModal::FolderOf(root / "src/Core/Workspace.cpp", roots) == U"src/Core");
    TR_ASSERT(t, OpenFilesModal::FolderOf(root / "main.cpp", roots).empty());
    // Outside every root: the folder as it is
    auto outside = fs::canonical(fs::temp_directory_path()) / "elsewhere" / "a.cpp";
    TR_ASSERT(t, OpenFilesModal::FolderOf(outside, roots) == UnicodeHelper::utf8to32(outside.parent_path().string()));
    return kTR_Pass;
}

//
// The open files (docs/usability-round.md §1): which ones the header shows when they don't all fit, and the list's
// filter and folders - the parts that need no screen.
//
#include <testinterface.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Core/Editor.h"
#include "Core/Graphics/Headless/HLScreen.h"
#include "Core/UI/Views/RootView.h"

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
    DLL_EXPORT int test_openfiles_drawn(ITesting *t);
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
    // The drawn case reads the headless screen's grid
    Editor::Instance().OpenScreen();
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
    // ...and after others: no '...' in front of it, it starts the row
    auto wideLater = EditorHeaderView::LayoutEntries({5, 50, 5}, 1, 10);
    TR_ASSERT(t, (wideLater.idxFirst == 1) && (wideLater.idxEnd == 2));
    TR_ASSERT(t, !wideLater.isCutBefore && wideLater.isCutAfter);
    // Room for the '... | ' and the name: kept
    auto fitsLater = EditorHeaderView::LayoutEntries({5, 8, 5}, 1, 14);
    TR_ASSERT(t, (fitsLater.idxFirst == 1) && fitsLater.isCutBefore);
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
                // A cut start shows '...' - unless the active one alone has no room for it, then it starts the row
                bool isActiveAloneTooWide = (span.idxEnd - span.idxFirst == 1) && ((widths[idxActive] + 6) > width);
                TR_ASSERT(t, span.isCutBefore == ((span.idxFirst > 0) && !isActiveAloneTooWide));
                TR_ASSERT(t, span.isCutAfter == (span.idxEnd < numEntries));
                TR_ASSERT(t, (DrawnWidth(widths, span) <= width) || (span.idxEnd - span.idxFirst == 1));
            }
        }
    }
    return kTR_Pass;
}

namespace {
    // Open files in the editor's own workspace, the first active - closed again when it goes out of scope
    struct OpenFiles {
        std::vector<Document::Ref> documents;
        std::vector<std::string> names;
        Document::Ref prevActive;

        OpenFiles(ITesting *t, const std::vector<std::string> &fileNames) {
            auto workspace = Editor::Instance().GetWorkspace();
            prevActive = Editor::Instance().GetActiveDocument();
            auto root = fs::canonical(fs::temp_directory_path()) / "goatedit_openfiles_drawn";
            fs::create_directories(root);
            for (auto &name : fileNames) {
                std::ofstream(root / name) << "x\n";
                auto node = workspace->NewDocumentWithFileRef(root / name);
                TR_ASSERT(t, node != nullptr);
                workspace->AddOpenDocument(node->GetDocument());
                documents.push_back(node->GetDocument());
                names.push_back(node->GetDisplayName());
            }
        }
        ~OpenFiles() {
            auto workspace = Editor::Instance().GetWorkspace();
            for (auto &document : documents) {
                workspace->RemoveOpenDocument(document);
            }
            Editor::Instance().SetActiveDocument(prevActive);
        }
    };
}

// The header drawn (docs/usability-round.md §5b): whatever the width and the active file, the active name is drawn
// whole and underlined, the row fits, and '...' is there exactly when a name is left out
DLL_EXPORT int test_openfiles_drawn(ITesting *t) {
    auto screen = HLScreen::FromRuntime();
    TR_ASSERT(t, screen != nullptr);
    OpenFiles files(t, {"alpha.cpp", "beta.h", "gamma_long_name.cpp", "delta.cpp", "epsilon.h", "zeta.cpp"});

    for (int width = 20; width <= 90; width += 7) {
        for (size_t idxActive = 0; idxActive < files.documents.size(); idxActive++) {
            Editor::Instance().SetActiveDocument(files.documents[idxActive]);
            RootView rootView;
            EditorHeaderView header;
            header.SetViewRect(gedit::Rect(gedit::Point(0, 0), width, 1));
            rootView.AddView(&header);
            rootView.Initialize();
            screen->ClearGrid();
            rootView.Draw();

            auto row = screen->Dump(gedit::Rect(gedit::Point(0, 0), width, 1));
            row.pop_back();     // the '\n'
            TR_ASSERT(t, row.rfind("[Files] ", 0) == 0);
            // Nothing past the header's width
            TR_ASSERT(t, screen->CellAt(width, 0).ch == U' ');

            // A name too wide for the row starts right after '[Files] ' - the screen cuts its end, never its start
            auto &active = files.names[idxActive];
            if ((8 + (int)active.length() + 1) > width) {
                auto shown = std::min(active.length(), (size_t)(width - 8));
                TR_ASSERT(t, row.substr(8, shown) == active.substr(0, shown));
                continue;
            }
            auto at = row.find(active);
            TR_ASSERT(t, at != std::string::npos);
            for (size_t x = at; x < at + active.length(); x++) {
                TR_ASSERT(t, gnilk::ansi::HasAttr(screen->CellAt((int)x, 0).attr, gnilk::ansi::Attr::Underline));
            }
            // A name left out shows as '...'; all in, none - every open document, not only these. Only with room for a
            // '... | ' on both sides: else the one after it may be past the edge
            if ((8 + 2 * 6 + (int)active.length() + 1) > width) {
                continue;
            }
            bool isAllShown = true;
            for (auto &document : Editor::Instance().GetDocuments()) {
                auto node = Editor::Instance().GetWorkspace()->GetNodeFromDocument(document);
                isAllShown = isAllShown && (row.find(node->GetDisplayName()) != std::string::npos);
            }
            TR_ASSERT(t, isAllShown == (row.find("...") == std::string::npos));
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

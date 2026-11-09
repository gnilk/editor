//
// The change marks (docs/usability-round.md §0c): the base from git, and a document's marks against it - unsaved
// edits included. Real temporary repositories.
//
#include <testinterface.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Core/Config/Config.h"
#include "Core/Editor.h"
#include "Core/Editor/Views/GutterView.h"
#include "Core/Graphics/Headless/HLScreen.h"
#include "Core/UI/Views/RootView.h"
#include "Core/Git/GitBase.h"
#include "Core/LineDiff.h"
#include "Core/Workspace.h"

using namespace gedit;
namespace fs = std::filesystem;

extern "C" {
    DLL_EXPORT int test_changemarks(ITesting *t);
    DLL_EXPORT int test_changemarks_base(ITesting *t);
    DLL_EXPORT int test_changemarks_new(ITesting *t);
    DLL_EXPORT int test_changemarks_none(ITesting *t);
    DLL_EXPORT int test_changemarks_renamed(ITesting *t);
    DLL_EXPORT int test_changemarks_split(ITesting *t);
    DLL_EXPORT int test_changemarks_document(ITesting *t);
    DLL_EXPORT int test_changemarks_alladded(ITesting *t);
    DLL_EXPORT int test_changemarks_cleared(ITesting *t);
    DLL_EXPORT int test_changemarks_drawn(ITesting *t);
}

namespace {
    fs::path MakeRoot(const std::string &name) {
        auto root = fs::canonical(fs::temp_directory_path()) / ("goatedit_changemarks_" + name);
        fs::remove_all(root);
        fs::create_directories(root);
        return root;
    }

    void WriteFile(const fs::path &path, const std::string &text) {
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file << text;
    }

    bool Git(const fs::path &root, const std::string &arguments) {
        auto command = "git -C '" + root.string() + "' -c user.name=test -c user.email=test@test -c commit.gpgsign=false "
                       + arguments + " >/dev/null 2>&1";
        return std::system(command.c_str()) == 0;
    }

    // A repository with 'src/file.txt' committed
    fs::path MakeRepo(ITesting *t, const std::string &name, const std::string &text) {
        auto root = MakeRoot(name);
        TR_ASSERT(t, Git(root, "init -q"));
        WriteFile(root / "src" / "file.txt", text);
        TR_ASSERT(t, Git(root, "add ."));
        TR_ASSERT(t, Git(root, "commit -q -m first"));
        return root;
    }

    // The marks as a string: '.' none, 'A' added, 'M' modified, then '^' / 'v' for a removal above / below
    std::string Marks(const LineDiff::Changes::Ref &changes) {
        std::string out;
        if (changes == nullptr) {
            return "null";
        }
        for (auto mark : changes->marks) {
            out += (mark & LineDiff::kAdded) ? 'A' : (mark & LineDiff::kModified) ? 'M' : '.';
            if (mark & LineDiff::kRemovedAbove) {
                out += '^';
            }
            if (mark & LineDiff::kRemovedBelow) {
                out += 'v';
            }
        }
        return out;
    }

    struct OpenDocument {
        Workspace workspace;
        Document::Ref document = nullptr;

        OpenDocument(ITesting *t, const fs::path &path) {
            Config::Instance()["main"].SetBool("threaded_syntaxparser", false);
            auto node = workspace.NewDocumentWithFileRef(path);
            TR_ASSERT(t, node != nullptr);
            TR_ASSERT(t, node->LoadData());
            document = node->GetDocument();
        }

        // An edit as the editor makes one: the line changed, its region parsed
        void Edit(size_t idxLine, const std::u32string &text) {
            auto line = document->GetTextBuffer()->LineAt(idxLine);
            line->Delete(0, (int)line->Length());
            line->Append(text);
            document->GetTextBuffer()->ReparseRegion(idxLine, idxLine + 1);
        }
        void InsertLine(size_t idxLine, const std::u32string &text) {
            document->GetTextBuffer()->Insert(idxLine, text);
            document->GetTextBuffer()->ReparseRegion(idxLine, idxLine + 1);
        }
    };
}

DLL_EXPORT int test_changemarks(ITesting *t) {
    // The drawn case reads the headless screen's grid
    Editor::Instance().OpenScreen();
    return kTR_Pass;
}

DLL_EXPORT int test_changemarks_base(ITesting *t) {
    auto root = MakeRepo(t, "base", "one\ntwo\nthree\n");
    // Changed on disk: the base is still the commit's
    WriteFile(root / "src" / "file.txt", "one\nTWO\nthree\n");
    auto result = GitBase::QueryAndWait(root / "src" / "file.txt");
    TR_ASSERT(t, result.kind == GitBase::kKind::kBase);
    TR_ASSERT(t, result.text == "one\ntwo\nthree\n");
    return kTR_Pass;
}

DLL_EXPORT int test_changemarks_new(ITesting *t) {
    auto root = MakeRepo(t, "new", "x\n");
    // Untracked
    WriteFile(root / "src" / "new.txt", "a\n");
    TR_ASSERT(t, GitBase::QueryAndWait(root / "src" / "new.txt").kind == GitBase::kKind::kAllAdded);
    // Added and staged, not committed
    WriteFile(root / "staged.txt", "a\n");
    TR_ASSERT(t, Git(root, "add staged.txt"));
    TR_ASSERT(t, GitBase::QueryAndWait(root / "staged.txt").kind == GitBase::kKind::kAllAdded);
    // A repository with no commit yet
    auto empty = MakeRoot("new_nocommit");
    TR_ASSERT(t, Git(empty, "init -q"));
    WriteFile(empty / "a.txt", "a\n");
    TR_ASSERT(t, GitBase::QueryAndWait(empty / "a.txt").kind == GitBase::kKind::kAllAdded);
    return kTR_Pass;
}

DLL_EXPORT int test_changemarks_none(ITesting *t) {
    auto root = MakeRepo(t, "none", "x\n");
    // Ignored
    WriteFile(root / ".gitignore", "*.log\n");
    WriteFile(root / "build.log", "a\n");
    TR_ASSERT(t, GitBase::QueryAndWait(root / "build.log").kind == GitBase::kKind::kNone);
    // Not in a repository
    auto outside = MakeRoot("none_outside");
    WriteFile(outside / "a.txt", "a\n");
    TR_ASSERT(t, GitBase::QueryAndWait(outside / "a.txt").kind == GitBase::kKind::kNone);
    // Binary
    std::string binary("a\0b\n", 4);
    WriteFile(root / "bin.dat", binary);
    TR_ASSERT(t, Git(root, "add bin.dat"));
    TR_ASSERT(t, Git(root, "commit -q -m bin"));
    TR_ASSERT(t, GitBase::QueryAndWait(root / "bin.dat").kind == GitBase::kKind::kNone);
    return kTR_Pass;
}

DLL_EXPORT int test_changemarks_renamed(ITesting *t) {
    // Renamed and staged: the base is the old name's text, or at worst all added - never nothing
    auto root = MakeRepo(t, "renamed", "one\ntwo\n");
    TR_ASSERT(t, Git(root, "mv src/file.txt src/moved.txt"));
    auto result = GitBase::QueryAndWait(root / "src" / "moved.txt");
    TR_ASSERT(t, result.kind != GitBase::kKind::kNone);
    if (result.kind == GitBase::kKind::kBase) {
        TR_ASSERT(t, result.text == "one\ntwo\n");
    }
    return kTR_Pass;
}

DLL_EXPORT int test_changemarks_split(ITesting *t) {
    // The base splits as a load does: a last '\n' adds no line, trailing white space and '\r' go, empty is one line
    TR_ASSERT(t, Document::HashLines("a\nb\n")->size() == 2);
    TR_ASSERT(t, Document::HashLines("a\nb")->size() == 2);
    TR_ASSERT(t, Document::HashLines("")->size() == 1);
    TR_ASSERT(t, (*Document::HashLines("a \r\n"))[0] == Line::HashOf(U"a"));
    return kTR_Pass;
}

DLL_EXPORT int test_changemarks_document(ITesting *t) {
    auto root = MakeRepo(t, "document", "one\ntwo\nthree\nfour\n");
    auto path = root / "src" / "file.txt";
    // On disk before it opens: line 2 changed
    WriteFile(path, "one\nTWO\nthree\nfour\n");
    OpenDocument open(t, path);
    auto result = GitBase::QueryAndWait(path);
    TR_ASSERT(t, result.kind == GitBase::kKind::kBase);
    open.document->SetChangeBase(Document::HashLines(result.text));
    TR_ASSERT(t, Marks(open.document->GetLineChanges()) == ".M..");

    // Unsaved edits: the marks follow the buffer
    open.Edit(3, U"FOUR");
    TR_ASSERT(t, Marks(open.document->GetLineChanges()) == ".M.M");
    open.InsertLine(0, U"zero");
    TR_ASSERT(t, Marks(open.document->GetLineChanges()) == "A.M.M");
    // Changed back to what the commit has: unmarked
    open.Edit(2, U"two");
    TR_ASSERT(t, Marks(open.document->GetLineChanges()) == "A...M");
    return kTR_Pass;
}

DLL_EXPORT int test_changemarks_alladded(ITesting *t) {
    auto root = MakeRoot("alladded");
    WriteFile(root / "a.txt", "a\nb\nc\n");
    OpenDocument open(t, root / "a.txt");
    open.document->SetChangeBaseAllAdded();
    TR_ASSERT(t, Marks(open.document->GetLineChanges()) == "AAA");
    open.InsertLine(1, U"x");
    TR_ASSERT(t, Marks(open.document->GetLineChanges()) == "AAAA");
    return kTR_Pass;
}

DLL_EXPORT int test_changemarks_cleared(ITesting *t) {
    auto root = MakeRoot("cleared");
    WriteFile(root / "a.txt", "a\nb\n");
    OpenDocument open(t, root / "a.txt");
    // No base: no marks, and an edit makes none
    TR_ASSERT(t, open.document->GetLineChanges() == nullptr);
    open.Edit(0, U"x");
    TR_ASSERT(t, open.document->GetLineChanges() == nullptr);
    open.document->SetChangeBase(Document::HashLines("a\nb\n"));
    TR_ASSERT(t, Marks(open.document->GetLineChanges()) == "M.");
    open.document->ClearChangeBase();
    open.Edit(1, U"y");
    TR_ASSERT(t, open.document->GetLineChanges() == nullptr);
    return kTR_Pass;
}

// The gutter drawn (docs/usability-round.md §5b): the mark in its column on the rows that changed, nothing on the
// others, and added and changed in colours of their own
DLL_EXPORT int test_changemarks_drawn(ITesting *t) {
    auto screen = HLScreen::FromRuntime();
    TR_ASSERT(t, screen != nullptr);
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);
    auto root = MakeRoot("drawn");
    WriteFile(root / "a.txt", "one\ntwo\nthree\nfour\nfive\n");

    auto workspace = Editor::Instance().GetWorkspace();
    auto prevActive = Editor::Instance().GetActiveDocument();
    auto node = workspace->NewDocumentWithFileRef(root / "a.txt");
    TR_ASSERT(t, (node != nullptr) && node->LoadData());
    auto document = node->GetDocument();
    workspace->AddOpenDocument(document);
    Editor::Instance().SetActiveDocument(document);
    document->OnViewInit(gedit::Rect(40, 20));
    // Against a base: 'two' changed, 'x' added after 'three', 'five' removed (end)
    document->SetChangeBase(Document::HashLines("one\nTWO\nthree\nfour\nfive\n"));
    document->GetTextBuffer()->Insert(3, U"x");
    document->GetTextBuffer()->ReparseRegion(3, 4);
    document->GetTextBuffer()->DeleteLineAt(5);
    document->GetTextBuffer()->ReparseRegion(4, 5);

    RootView rootView;
    GutterView gutter;
    gutter.SetViewRect(gedit::Rect(gedit::Point(0, 0), 10, 20));
    rootView.AddView(&gutter);
    rootView.Initialize();
    screen->ClearGrid();
    rootView.Draw();

    // Lines: one . | two M | three . | x A | four v
    const int col = GutterView::CHANGE_MARK_COLUMN;
    TR_ASSERT(t, screen->CellAt(col, 0).ch == U' ');
    TR_ASSERT(t, screen->CellAt(col, 1).ch == U'\u2502');
    TR_ASSERT(t, screen->CellAt(col, 2).ch == U' ');
    TR_ASSERT(t, screen->CellAt(col, 3).ch == U'\u2502');
    TR_ASSERT(t, screen->CellAt(col, 4).ch == U'_');
    TR_ASSERT(t, screen->CellAt(col, 5).ch == U' ');
    // Changed and added apart, and neither in the numbers' colour
    TR_ASSERT(t, screen->CellAt(col, 1).fg != screen->CellAt(col, 3).fg);
    TR_ASSERT(t, screen->CellAt(col, 1).fg != screen->CellAt(5, 1).fg);
    // The number is still there
    TR_ASSERT(t, screen->CellAt(5, 1).ch == U'1');

    workspace->RemoveOpenDocument(document);
    Editor::Instance().SetActiveDocument(prevActive);
    return kTR_Pass;
}

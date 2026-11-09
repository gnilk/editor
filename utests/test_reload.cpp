//
// Reloading a file changed on disk (docs/usability-round.md §0b): an open document on a temp folder, the file rewritten
// behind it - as an agent or 'git checkout' would - and the folder monitor's flush stood in for by a direct call.
//
#include <testinterface.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "Core/Config/Config.h"
#include "Core/Workspace.h"

using namespace gedit;
namespace fs = std::filesystem;

extern "C" {
    DLL_EXPORT int test_reload(ITesting *t);
    DLL_EXPORT int test_reload_clean(ITesting *t);
    DLL_EXPORT int test_reload_keepsunchanged(ITesting *t);
    DLL_EXPORT int test_reload_undo(ITesting *t);
    DLL_EXPORT int test_reload_edges(ITesting *t);
    DLL_EXPORT int test_reload_shorter(ITesting *t);
    DLL_EXPORT int test_reload_dirty(ITesting *t);
    DLL_EXPORT int test_reload_ownsave(ITesting *t);
    DLL_EXPORT int test_reload_cleansave(ITesting *t);
    DLL_EXPORT int test_reload_autosave(ITesting *t);
    DLL_EXPORT int test_reload_otherfolder(ITesting *t);
}

namespace {
    fs::path MakeRoot(const std::string &name) {
        auto root = fs::canonical(fs::temp_directory_path()) / ("goatedit_reload_" + name);
        fs::remove_all(root);
        fs::create_directories(root);
        return root;
    }

    // Written as another program would, a second later by the clock - a rewrite within the same tick of the file
    // system's clock with the same size would otherwise look unchanged
    void WriteLines(const fs::path &path, const std::vector<std::string> &lines) {
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            for (auto &line : lines) {
                file << line << "\n";
            }
        }
        static int numWrites = 0;
        numWrites++;
        fs::last_write_time(path, fs::file_time_type::clock::now() + std::chrono::seconds(numWrites));
    }

    std::vector<std::string> ReadBack(const fs::path &path) {
        std::vector<std::string> lines;
        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        return lines;
    }

    std::vector<std::string> BufferLines(const Document::Ref &document) {
        std::vector<std::string> lines;
        for (auto &line : document->Lines()) {
            lines.push_back(UnicodeHelper::utf32to8(line->Buffer()));
        }
        return lines;
    }

    // One open document on a file in its own folder
    struct Fixture {
        fs::path root;
        fs::path path;
        Workspace workspace;
        Workspace::Node::Ref node = nullptr;
        Document::Ref document = nullptr;

        Fixture(ITesting *t, const std::string &name, const std::vector<std::string> &lines) {
            Config::Instance()["main"].SetBool("threaded_syntaxparser", false);
            root = MakeRoot(name);
            path = root / "file.txt";
            WriteLines(path, lines);
            node = workspace.NewDocumentWithFileRef(path);
            TR_ASSERT(t, node != nullptr);
            TR_ASSERT(t, node->LoadData());
            document = node->GetDocument();
            document->OnViewInit(gedit::Rect(80, 20));
            if (!workspace.IsDocumentOpen(document)) {
                workspace.AddOpenDocument(document);
            }
        }

        // What the folder monitor's flush does for the file's folder
        void Flush() {
            workspace.CheckOpenDocumentsOnDisk({root}, false);
        }
    };

    std::vector<std::string> Numbered(int count, const std::string &prefix = "line ") {
        std::vector<std::string> lines;
        for (int i = 0; i < count; i++) {
            lines.push_back(prefix + std::to_string(i));
        }
        return lines;
    }
}

DLL_EXPORT int test_reload(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_reload_clean(ITesting *t) {
    Fixture fixture(t, "clean", {"one", "two", "three"});
    TR_ASSERT(t, !fixture.document->IsChangedOnDisk());

    WriteLines(fixture.path, {"one", "TWO", "three", "four"});
    TR_ASSERT(t, fixture.document->IsChangedOnDisk());
    fixture.Flush();
    TR_ASSERT(t, (BufferLines(fixture.document) == std::vector<std::string>{"one", "TWO", "three", "four"}));
    // In step with the file: nothing to save, nothing changed on disk any more
    TR_ASSERT(t, !fixture.document->IsModified());
    TR_ASSERT(t, !fixture.document->IsChangedOnDisk());
    TR_ASSERT(t, !fixture.document->HasConflictOnDisk());
    return kTR_Pass;
}

DLL_EXPORT int test_reload_keepsunchanged(ITesting *t) {
    // Only what differs is replaced: the lines before and after are the same objects, colours and all
    Fixture fixture(t, "keepsunchanged", Numbered(10));
    auto first = fixture.document->LineAt(0);
    auto last = fixture.document->LineAt(9);
    auto middle = fixture.document->LineAt(5);
    auto lines = Numbered(10);
    lines[5] = "changed";
    WriteLines(fixture.path, lines);
    fixture.Flush();
    TR_ASSERT(t, BufferLines(fixture.document) == lines);
    TR_ASSERT(t, fixture.document->LineAt(0) == first);
    TR_ASSERT(t, fixture.document->LineAt(9) == last);
    TR_ASSERT(t, fixture.document->LineAt(5) != middle);
    return kTR_Pass;
}

DLL_EXPORT int test_reload_undo(ITesting *t) {
    // A reload is one undo step: what another program wrote can be stepped back
    Fixture fixture(t, "undo", {"a", "b", "c"});
    WriteLines(fixture.path, {"a", "x", "y", "c"});
    fixture.Flush();
    TR_ASSERT(t, (BufferLines(fixture.document) == std::vector<std::string>{"a", "x", "y", "c"}));
    size_t idxActiveLine = 0;
    fixture.document->Undo(fixture.document->GetCursor(), idxActiveLine);
    TR_ASSERT(t, (BufferLines(fixture.document) == std::vector<std::string>{"a", "b", "c"}));
    return kTR_Pass;
}

DLL_EXPORT int test_reload_edges(ITesting *t) {
    // Lines only added, at the start and at the end; lines only removed; everything replaced; emptied
    struct Case {
        std::vector<std::string> before;
        std::vector<std::string> after;
    };
    std::vector<Case> cases = {
        {{"a", "b"}, {"new", "a", "b"}},
        {{"a", "b"}, {"a", "b", "new"}},
        {{"a", "b", "c"}, {"a", "c"}},
        {{"a", "b", "c"}, {"c"}},
        {{"a", "b", "c"}, {"a"}},
        {{"a", "b"}, {"x", "y", "z"}},
        {{"a", "b"}, {}},
        {{"a"}, {"a", "a", "a"}},
    };
    int idxCase = 0;
    for (auto &testCase : cases) {
        Fixture fixture(t, "edges" + std::to_string(idxCase++), testCase.before);
        WriteLines(fixture.path, testCase.after);
        fixture.Flush();
        // An empty file is one empty line, as a load gives it
        auto expected = testCase.after.empty() ? std::vector<std::string>{""} : testCase.after;
        TR_ASSERT(t, BufferLines(fixture.document) == expected);
    }
    return kTR_Pass;
}

DLL_EXPORT int test_reload_shorter(ITesting *t) {
    Fixture fixture(t, "shorter", Numbered(10, "a long line number "));
    fixture.document->SetCursorPosition(8, 15);
    WriteLines(fixture.path, {"x", "yy", "zzz"});
    fixture.Flush();
    TR_ASSERT(t, fixture.document->Lines().size() == 3);
    // On what is left: the last line, within it
    TR_ASSERT(t, fixture.document->GetLineCursor().idxActiveLine == 2);
    TR_ASSERT(t, fixture.document->GetCursor().position.x <= 3);
    return kTR_Pass;
}

DLL_EXPORT int test_reload_dirty(ITesting *t) {
    // Unsaved edits are never thrown away: kept, marked, and the file on disk isn't touched
    Fixture fixture(t, "dirty", {"one", "two"});
    fixture.document->ReplaceLineRange(0, 0, {U"my edit"});
    TR_ASSERT(t, fixture.document->IsModified());
    WriteLines(fixture.path, {"theirs"});
    fixture.Flush();
    TR_ASSERT(t, (BufferLines(fixture.document) == std::vector<std::string>{"my edit", "two"}));
    TR_ASSERT(t, fixture.document->HasConflictOnDisk());
    TR_ASSERT(t, (ReadBack(fixture.path) == std::vector<std::string>{"theirs"}));
    return kTR_Pass;
}

DLL_EXPORT int test_reload_ownsave(ITesting *t) {
    // Our own save is not a change on disk
    Fixture fixture(t, "ownsave", {"one"});
    fixture.document->ReplaceLineRange(0, 0, {U"saved"});
    TR_ASSERT(t, fixture.node->SaveData());
    TR_ASSERT(t, !fixture.document->IsChangedOnDisk());
    fixture.Flush();
    TR_ASSERT(t, (BufferLines(fixture.document) == std::vector<std::string>{"saved"}));
    TR_ASSERT(t, !fixture.document->HasConflictOnDisk());
    return kTR_Pass;
}

DLL_EXPORT int test_reload_cleansave(ITesting *t) {
    // A save of an unchanged buffer writes nothing - it mustn't take the rewritten file as ours, or it never reloads
    Fixture fixture(t, "cleansave", {"one"});
    WriteLines(fixture.path, {"theirs"});
    TR_ASSERT(t, fixture.node->SaveData());
    TR_ASSERT(t, fixture.document->IsChangedOnDisk());
    fixture.Flush();
    TR_ASSERT(t, (BufferLines(fixture.document) == std::vector<std::string>{"theirs"}));
    return kTR_Pass;
}

DLL_EXPORT int test_reload_autosave(ITesting *t) {
    // The autosave comes seconds after the last key: it must not write over a file rewritten meanwhile, even before
    // the folder monitor has said so
    Fixture fixture(t, "autosave", {"one"});
    fixture.document->ReplaceLineRange(0, 0, {U"my edit"});
    WriteLines(fixture.path, {"theirs"});
    TR_ASSERT(t, !fixture.node->AutoSaveData());
    TR_ASSERT(t, (ReadBack(fixture.path) == std::vector<std::string>{"theirs"}));
    TR_ASSERT(t, fixture.document->HasConflictOnDisk());
    TR_ASSERT(t, fixture.document->IsModified());

    // With nothing rewritten, the autosave saves as before
    Fixture other(t, "autosave_plain", {"one"});
    other.document->ReplaceLineRange(0, 0, {U"my edit"});
    TR_ASSERT(t, other.node->AutoSaveData());
    TR_ASSERT(t, (ReadBack(other.path) == std::vector<std::string>{"my edit"}));
    return kTR_Pass;
}

DLL_EXPORT int test_reload_otherfolder(ITesting *t) {
    // A flush for another folder leaves the document alone; lost events check every open document
    Fixture fixture(t, "otherfolder", {"one"});
    WriteLines(fixture.path, {"theirs"});
    fixture.workspace.CheckOpenDocumentsOnDisk({fixture.root / "elsewhere"}, false);
    TR_ASSERT(t, (BufferLines(fixture.document) == std::vector<std::string>{"one"}));
    fixture.workspace.CheckOpenDocumentsOnDisk({}, true);
    TR_ASSERT(t, (BufferLines(fixture.document) == std::vector<std::string>{"theirs"}));
    return kTR_Pass;
}

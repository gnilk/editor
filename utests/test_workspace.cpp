//
// Created by gnilk on 09.05.23.
//
#include <testinterface.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "Core/Editor.h"
#include "Core/Workspace.h"
#include "Core/RuntimeConfig.h"
#include "Core/PathUtil.h"

using namespace gedit;
using namespace std::chrono_literals;

extern "C" {
DLL_EXPORT int test_workspace(ITesting *t);
DLL_EXPORT int test_workspace_empty(ITesting *t);
DLL_EXPORT int test_workspace_new(ITesting *t);
DLL_EXPORT int test_workspace_newtwice(ITesting *t);
DLL_EXPORT int test_workspace_fileref(ITesting *t);
DLL_EXPORT int test_workspace_newdocument(ITesting *t);
DLL_EXPORT int test_workspace_openfolder(ITesting *t);
DLL_EXPORT int test_workspace_openfolder_lazy(ITesting *t);
DLL_EXPORT int test_workspace_openabsfolder(ITesting *t);
DLL_EXPORT int test_workspace_openfolder_excludes_builddir(ITesting *t);
DLL_EXPORT int test_workspace_isscanned(ITesting *t);
DLL_EXPORT int test_workspace_scannode(ITesting *t);
DLL_EXPORT int test_workspace_activefolder(ITesting *t);
DLL_EXPORT int test_workspace_newsubfolder(ITesting *t);
DLL_EXPORT int test_workspace_reopen(ITesting *t);
DLL_EXPORT int test_workspace_removedocument(ITesting *t);
DLL_EXPORT int test_workspace_recreate(ITesting *t);
DLL_EXPORT int test_workspace_canonicalroot(ITesting *t);
DLL_EXPORT int test_workspace_gonefromdisk(ITesting *t);
DLL_EXPORT int test_workspace_monitorexclude(ITesting *t);
DLL_EXPORT int test_workspace_scannode_keepsopen(ITesting *t);
DLL_EXPORT int test_workspace_reconcile(ITesting *t);
DLL_EXPORT int test_workspace_monitorflush(ITesting *t);
DLL_EXPORT int test_workspace_monitorlifetime(ITesting *t);
DLL_EXPORT int test_workspace_tabnumber(ITesting *t);
}

DLL_EXPORT int test_workspace(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_workspace_empty(ITesting *t) {
    Workspace workspace;
    TR_ASSERT(t, workspace.GetProjectRoots().size() == 0);
    return kTR_Pass;
}
DLL_EXPORT int test_workspace_new(ITesting *t) {
    Workspace workspace;
    TR_ASSERT(t, workspace.GetProjectRoots().size() == 0);
    auto node = workspace.NewDocument("dummy");
    TR_ASSERT(t, workspace.GetProjectRoots().size() != 0);
    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetDocuments().size() != 0);
    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetDocuments().size() == 1);
    for(auto &m : workspace.GetDefaultWorkspace()->GetDocuments()) {
        auto node = workspace.GetNodeFromDocument(m);

        TR_ASSERT(t, node->GetDisplayName() == "dummy");
    }

    return kTR_Pass;
}

DLL_EXPORT int test_workspace_newtwice(ITesting *t) {
    Workspace workspace;
    TR_ASSERT(t, workspace.GetProjectRoots().size() == 0);
    workspace.NewDocument("m1");
    workspace.NewDocument("m2");
    TR_ASSERT(t, workspace.GetProjectRoots().size() != 0);
    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetDocuments().size() != 0);
    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetDocuments().size() == 2);

    auto documents = workspace.GetDefaultWorkspace()->GetDocuments();
    int count = 0;
    // We can't really guarantee the return order there
    for(auto &m : workspace.GetDefaultWorkspace()->GetDocuments()) {
        auto node = workspace.GetNodeFromDocument(m);
        char buffer[64];
        snprintf(buffer,63,"new_%d", count);
        printf("Name: %s == %s\n", buffer,node->GetDisplayName().c_str());
        count++;
    }
    return kTR_Pass;
}

DLL_EXPORT int test_workspace_newdocument(ITesting *t) {
    Workspace workspace;
    auto node = workspace.NewDocument("wef");

    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetDocuments().size() != 0);
    TR_ASSERT(t, node->GetTextBuffer()->GetBufferState() == TextBuffer::BufferState::kBuffer_FileRef);

    return kTR_Pass;
}

DLL_EXPORT int test_workspace_fileref(ITesting *t) {
    Workspace workspace;
    std::filesystem::path filename("testfiles/ConvertUTF.cpp");
    auto node = workspace.NewDocumentWithFileRef(filename);

    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetDocuments().size() != 0);
    TR_ASSERT(t, node->GetTextBuffer()->GetBufferState() == TextBuffer::BufferState::kBuffer_FileRef);
    TR_ASSERT(t, node->LoadData());
    TR_ASSERT(t, node->GetTextBuffer()->GetBufferState() == TextBuffer::BufferState::kBuffer_Loaded);

    return kTR_Pass;
}

DLL_EXPORT int test_workspace_openfolder(ITesting *t) {
    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder("."));
    auto desktops = workspace.GetProjectRoots();
    TR_ASSERT(t, desktops.size() == 1);
    auto desktop = desktops[0];
    auto rootNode = desktop->GetRootNode();
    // Lazy tree: scanning a folder builds path-only nodes - NO documents exist until a node is opened.
    TR_ASSERT(t, rootNode->GetNumChildNodes() > 0);
    TR_ASSERT(t, rootNode->GetDocuments().size() == 0);
    return kTR_Pass;
}

// Opening a scanned (path-only) file node lazily creates exactly one document, and re-opening reuses it.
DLL_EXPORT int test_workspace_openfolder_lazy(ITesting *t) {
    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder("."));
    auto desktop = workspace.GetProjectRoots()[0];
    auto rootNode = desktop->GetRootNode();

    // Find a file node among the root's immediate children
    std::vector<Workspace::Node::Ref> children;
    rootNode->FlattenChilds(children);
    Workspace::Node::Ref fileNode = nullptr;
    for (auto &child : children) {
        if (child->GetMeta<int>(Workspace::Node::kMetaKey_NodeType, Workspace::Node::kNodeFolder) == Workspace::Node::kNodeFileRef) {
            fileNode = child;
            break;
        }
    }
    TR_ASSERT(t, fileNode != nullptr);

    // Path-only until opened
    TR_ASSERT(t, fileNode->GetDocument() == nullptr);

    auto document = workspace.EnsureDocumentForNode(fileNode);
    TR_ASSERT(t, document != nullptr);
    TR_ASSERT(t, fileNode->GetDocument() == document);
    // The document adopts the node's path as its identity
    TR_ASSERT(t, document->GetPath() == fileNode->GetNodePath());
    // Re-ensuring reuses the same document (does not rebuild)
    TR_ASSERT(t, workspace.EnsureDocumentForNode(fileNode) == document);

    return kTR_Pass;
}

DLL_EXPORT int test_workspace_openabsfolder(ITesting *t) {
    Workspace workspace;
#ifdef GEDIT_LINUX
    workspace.OpenFolder("/home/gnilk/src/test/sdl2test");
#endif
    return kTR_Pass;

}


// Collect display names of every node in the subtree (FlattenChilds is shallow, so recurse).
static void CollectNodeNames(const Workspace::Node::Ref &node, std::vector<std::string> &out) {
    std::vector<Workspace::Node::Ref> kids;
    node->FlattenChilds(kids);
    for (auto &kid : kids) {
        out.push_back(kid->GetDisplayName());
        CollectNodeNames(kid, out);
    }
}

static bool Contains(const std::vector<std::string> &names, const std::string &name) {
    return std::find(names.begin(), names.end(), name) != names.end();
}

// FS-3 / open-bugs #4: OpenFolder now drives the FolderScanner, which prunes the configured exclude
// names (workspace.exclude includes cmake-build-debug) at scan time. Opening a project root that
// contains a build-dir subtree must keep the source tree and leave the build dir (and everything under
// it) entirely out of the node tree. Discriminating: pre-fix OpenFolder ingested the build dir.
// Depth is set explicitly (4): this test is about exclude behaviour, not depth-limiting.
DLL_EXPORT int test_workspace_openfolder_excludes_builddir(ITesting *t) {
    namespace fs = std::filesystem;
    static int counter = 0;
    auto root = pathutil::CanonicalPath(fs::temp_directory_path() / ("goat_ws_exclude_" + std::to_string(counter++)));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "src");
    fs::create_directories(root / "cmake-build-debug" / "_deps");
    { std::ofstream(root / "src" / "main.cpp") << "x"; }
    { std::ofstream(root / "cmake-build-debug" / "build.ninja") << "x"; }
    { std::ofstream(root / "cmake-build-debug" / "_deps" / "dep.c") << "x"; }

    auto oldDepth = Config::Instance()["workspace"].GetInt("max_scan_depth", 1);
    Config::Instance()["workspace"].SetInt("max_scan_depth", 4);

    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder(root.string()));

    Config::Instance()["workspace"].SetInt("max_scan_depth", oldDepth);

    std::vector<std::string> names;
    CollectNodeNames(workspace.GetProjectRoots()[0]->GetRootNode(), names);

    // Build dir and everything beneath it pruned at scan time.
    TR_ASSERT(t, !Contains(names, "cmake-build-debug"));
    TR_ASSERT(t, !Contains(names, "_deps"));
    TR_ASSERT(t, !Contains(names, "build.ninja"));
    TR_ASSERT(t, !Contains(names, "dep.c"));
    // The real source tree (including the nested file) survives.
    TR_ASSERT(t, Contains(names, "src"));
    TR_ASSERT(t, Contains(names, "main.cpp"));

    fs::remove_all(root, ec);
    return kTR_Pass;
}

// FS-6 W2: Node::isScanned is written by the onLeaveDir adapter. With a low max_scan_depth,
// frontier folder nodes (emitted but not descended) carry isScanned==false; a descended-and-empty
// folder stays isScanned==true (the walked-empty case — expanding it is a known no-op, not a retry).
DLL_EXPORT int test_workspace_isscanned(ITesting *t) {
    namespace fs = std::filesystem;
    static int counter = 0;
    auto root = pathutil::CanonicalPath(fs::temp_directory_path() / ("goat_ws_isscanned_" + std::to_string(counter++)));
    std::error_code ec;
    fs::remove_all(root, ec);
    // a/b/deep.txt: "a" at depth 0 gets descended (isScanned=true), "b" at depth 1 hits the bound
    // (depth+1=2 >= maxDepth=2 → isScanned=false). "empty" at depth 0 is descended and empty → true.
    fs::create_directories(root / "a" / "b");
    fs::create_directories(root / "empty");
    { std::ofstream(root / "a" / "b" / "deep.txt") << "x"; }

    auto oldDepth = Config::Instance()["workspace"].GetInt("max_scan_depth", 64);
    Config::Instance()["workspace"].SetInt("max_scan_depth", 2);

    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder(root.string()));

    Config::Instance()["workspace"].SetInt("max_scan_depth", oldDepth);

    auto rootNode = workspace.GetProjectRoots()[0]->GetRootNode();

    auto nodeA     = rootNode->FindNodeWithPath(root / "a");
    auto nodeB     = rootNode->FindNodeWithPath(root / "a" / "b");
    auto nodeEmpty = rootNode->FindNodeWithPath(root / "empty");

    TR_ASSERT(t, nodeA     != nullptr);
    TR_ASSERT(t, nodeB     != nullptr);
    TR_ASSERT(t, nodeEmpty != nullptr);

    TR_ASSERT(t,  nodeA->IsScanned());       // descended (depth 0, depth+1=1 < maxDepth=2)
    TR_ASSERT(t, !nodeB->IsScanned());       // frontier: depth+1=2 >= maxDepth=2
    TR_ASSERT(t,  nodeEmpty->IsScanned());   // descended, walked-and-empty → still fully scanned

    fs::remove_all(root, ec);
    return kTR_Pass;
}

// FS-6 W3: ScanNode does a maxDepth=1 scan rooted at the given folder node.
//   - Before: frontier folder has isScanned==false, no children.
//   - After: node->isScanned==true; immediate children exist; child dirs have isScanned==false
//     (they are the next frontier); files are present.
//   - Idempotent: a second ScanNode call clears and rescans — same result.
DLL_EXPORT int test_workspace_scannode(ITesting *t) {
    namespace fs = std::filesystem;
    static int counter = 0;
    auto root = pathutil::CanonicalPath(fs::temp_directory_path() / ("goat_ws_scannode_" + std::to_string(counter++)));
    std::error_code ec;
    fs::remove_all(root, ec);
    // a/b/deep.txt — with max_scan_depth=1 the initial scan sees "a" (isScanned=false) but not "b".
    // ScanNode("a") should populate "b" and "leaf.txt" under "a", with "b" arriving isScanned=false.
    fs::create_directories(root / "a" / "b");
    { std::ofstream(root / "a" / "leaf.txt") << "x"; }
    { std::ofstream(root / "a" / "b" / "deep.txt") << "x"; }

    auto oldDepth = Config::Instance()["workspace"].GetInt("max_scan_depth", 64);
    Config::Instance()["workspace"].SetInt("max_scan_depth", 1);

    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder(root.string()));

    Config::Instance()["workspace"].SetInt("max_scan_depth", oldDepth);

    auto rootNode = workspace.GetProjectRoots()[0]->GetRootNode();
    auto nodeA = rootNode->FindNodeWithPath(root / "a");
    TR_ASSERT(t, nodeA != nullptr);
    TR_ASSERT(t, !nodeA->IsScanned());         // frontier: depth-bounded by initial scan
    TR_ASSERT(t, nodeA->GetNumChildNodes() == 0);

    workspace.ScanNode(nodeA);

    TR_ASSERT(t,  nodeA->IsScanned());         // ScanNode marks the target as fully scanned
    TR_ASSERT(t, nodeA->GetNumChildNodes() == 2);  // "b" (dir) + "leaf.txt" (file)

    auto nodeB = rootNode->FindNodeWithPath(root / "a" / "b");
    TR_ASSERT(t, nodeB != nullptr);
    TR_ASSERT(t, !nodeB->IsScanned());         // "b" is the new frontier (depth+1=1 >= maxDepth=1)

    auto leaf = rootNode->FindNodeWithPath(root / "a" / "leaf.txt");
    TR_ASSERT(t, leaf != nullptr);

    // Idempotent: second ScanNode produces the same result.
    workspace.ScanNode(nodeA);
    TR_ASSERT(t,  nodeA->IsScanned());
    TR_ASSERT(t, nodeA->GetNumChildNodes() == 2);
    auto nodeB2 = rootNode->FindNodeWithPath(root / "a" / "b");
    TR_ASSERT(t, nodeB2 != nullptr);
    TR_ASSERT(t, !nodeB2->IsScanned());

    fs::remove_all(root, ec);
    return kTR_Pass;
}

// A selected FILE makes its folder the active folder, and a new document lands in the active folder
// (absolute path below it) - never in the process CWD.
DLL_EXPORT int test_workspace_activefolder(ITesting *t) {
    namespace fs = std::filesystem;
    static int counter = 0;
    auto root = pathutil::CanonicalPath(fs::temp_directory_path() / ("goat_ws_activefolder_" + std::to_string(counter++)));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "a");
    { std::ofstream(root / "a" / "leaf.txt") << "x"; }

    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder(root.string()));
    auto rootNode = workspace.GetProjectRoots()[0]->GetRootNode();
    auto nodeA = rootNode->FindNodeWithPath(root / "a");
    auto leaf = rootNode->FindNodeWithPath(root / "a" / "leaf.txt");
    TR_ASSERT(t, (nodeA != nullptr) && (leaf != nullptr));

    workspace.SetActiveFolderNode(leaf);
    TR_ASSERT(t, workspace.GetActiveFolderNode() == nodeA);
    workspace.SetActiveFolderNode(nodeA);
    TR_ASSERT(t, workspace.GetActiveFolderNode() == nodeA);

    auto node = workspace.NewDocument("new.cpp");
    TR_ASSERT(t, node != nullptr);
    TR_ASSERT(t, node->GetNodePath() == root / "a" / "new.cpp");
    TR_ASSERT(t, node->GetParent() == nodeA);
    TR_ASSERT(t, node->GetDocument() != nullptr);

    // The name of a file that exists on disk reuses its node (opens it) rather than adding an empty twin
    auto existing = workspace.NewDocument("leaf.txt");
    TR_ASSERT(t, existing == leaf);

    fs::remove_all(root, ec);
    return kTR_Pass;
}

// '.nb sub/deeper/x.cpp': the folders are created (disk + tree), the document lands in the deepest one,
// which becomes the active folder. A folder name or an absolute path outside every root is handled too.
DLL_EXPORT int test_workspace_newsubfolder(ITesting *t) {
    namespace fs = std::filesystem;
    static int counter = 0;
    auto root = pathutil::CanonicalPath(fs::temp_directory_path() / ("goat_ws_newsub_" + std::to_string(counter++)));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);

    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder(root.string()));
    auto rootNode = workspace.GetProjectRoots()[0]->GetRootNode();
    workspace.SetActiveFolderNode(rootNode);

    auto node = workspace.NewDocument("sub/deeper/x.cpp");
    TR_ASSERT(t, node != nullptr);
    TR_ASSERT(t, node->GetNodePath() == root / "sub" / "deeper" / "x.cpp");
    TR_ASSERT(t, fs::is_directory(root / "sub" / "deeper"));
    TR_ASSERT(t, !fs::exists(root / "sub" / "deeper" / "x.cpp"));   // nothing written until saved

    auto deeper = rootNode->FindNodeWithPath(root / "sub" / "deeper");
    TR_ASSERT(t, deeper != nullptr);
    TR_ASSERT(t, node->GetParent() == deeper);
    TR_ASSERT(t, deeper->GetParent() == rootNode->FindNodeWithPath(root / "sub"));
    TR_ASSERT(t, workspace.GetActiveFolderNode() == deeper);

    // The next plain name lands in the (new) active folder
    auto next = workspace.NewDocument("y.cpp");
    TR_ASSERT(t, next->GetNodePath() == root / "sub" / "deeper" / "y.cpp");

    // '..' climbs back out, still inside the root - no duplicate 'sub' node
    auto up = workspace.NewDocument("../z.cpp");
    TR_ASSERT(t, up->GetNodePath() == root / "sub" / "z.cpp");
    TR_ASSERT(t, rootNode->GetNumChildNodes() == 1);

    // A name that is an existing folder isn't a document
    workspace.SetActiveFolderNode(rootNode);
    TR_ASSERT(t, workspace.NewDocument("sub") == nullptr);

    // Absolute path outside every root: a loose file under 'default', path kept, no folders created
    auto outside = pathutil::CanonicalPath(fs::temp_directory_path() / ("goat_ws_outside_" + std::to_string(counter)) / "o.cpp");
    auto loose = workspace.NewDocument(outside.string());
    TR_ASSERT(t, loose != nullptr);
    TR_ASSERT(t, fs::absolute(loose->GetNodePath()).lexically_normal() == outside);
    TR_ASSERT(t, !fs::exists(outside.parent_path()));

    fs::remove_all(root, ec);
    return kTR_Pass;
}

// Close then reopen a file shows the file as it is on disk - in both parser modes, and for an empty file
// (which never starts a parse thread). Closing must leave the buffer re-loadable.
static bool CloseAndReopen(ITesting *t, const std::filesystem::path &path, const std::string &expectFirstLine) {
    auto &editor = Editor::Instance();
    auto node = editor.GetWorkspace()->NewDocumentWithFileRef(path);
    auto document = editor.OpenDocumentFromWorkspace(node);
    if (document == nullptr) {
        return false;
    }
    document->GetTextBuffer()->AddLineUTF8("in memory only");   // an edit that was never saved
    if (!editor.CloseDocument(document)) {
        return false;
    }
    auto reopened = editor.OpenDocumentFromWorkspace(node);
    if (reopened == nullptr) {
        return false;
    }
    auto buffer = reopened->GetTextBuffer();
    bool isOk = (buffer->NumLines() > 0) && (buffer->LineAt(0)->BufferAsUTF8() == expectFirstLine) &&
                !reopened->IsModified();
    editor.CloseDocument(reopened);
    return isOk;
}

DLL_EXPORT int test_workspace_reopen(ITesting *t) {
    namespace fs = std::filesystem;
    auto root = (fs::temp_directory_path() / "goat_ws_reopen").lexically_normal();
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);
    { std::ofstream(root / "a.cpp") << "int a;\n"; }
    { std::ofstream(root / "b.cpp") << "int b;\n"; }
    { std::ofstream(root / "empty.cpp"); }

    // No autosave - the unsaved edit must still be unsaved at close (tests have no root view, so no timer
    // is created anyway - this keeps the test honest if that changes)
    auto oldAutoSave = Config::Instance()["main"].GetInt("autosave_timeout_ms", 0);
    Config::Instance()["main"].SetInt("autosave_timeout_ms", 0);
    auto oldThreaded = Config::Instance()["main"].GetBool("threaded_syntaxparser", false);

    Config::Instance()["main"].SetBool("threaded_syntaxparser", true);
    TR_ASSERT(t, CloseAndReopen(t, root / "a.cpp", "int a;"));
    TR_ASSERT(t, CloseAndReopen(t, root / "empty.cpp", ""));

    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);
    TR_ASSERT(t, CloseAndReopen(t, root / "b.cpp", "int b;"));

    Config::Instance()["main"].SetBool("threaded_syntaxparser", oldThreaded);
    Config::Instance()["main"].SetInt("autosave_timeout_ms", oldAutoSave);
    fs::remove_all(root, ec);
    return kTR_Pass;
}

DLL_EXPORT int test_workspace_removedocument(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", true);

    Workspace workspace;
    {
        auto node = workspace.NewDocument("wef");
        workspace.RemoveDocument(node->GetDocument());
    } // should lose the shared_ptr for the document when leaving this block...

    return kTR_Pass;
}

DLL_EXPORT int test_workspace_recreate(ITesting *t) {
    Workspace::Ref workspace = Workspace::Create();
    // Create a number of documents
    workspace->NewDocument("m1");
    workspace->NewDocument("m2");
    workspace->NewDocument("m2");

    // Let's see if all DTOR's are invoked
    // note: in order to test this - set breakpoints in DTORs
    workspace = nullptr;
    std::this_thread::sleep_for(500ms);
    return kTR_Pass;


}

// The tree's paths are canonical, whatever form the folder was opened with: 'goatedit .' gives '<dir>/.',
// and macOS temp dirs live behind a symlink (/var -> /private/var). The folder monitor reports canonical
// real paths, so an event for a direct child of the root must find the root as its parent.
DLL_EXPORT int test_workspace_canonicalroot(ITesting *t) {
    namespace fs = std::filesystem;
    auto root = fs::temp_directory_path() / "goat_ws_canonical";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "a");
    { std::ofstream(root / "a" / "leaf.txt") << "x"; }
    auto canonicalRoot = fs::canonical(root);

    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder((root / ".").string()));
    auto rootNode = workspace.GetProjectRoots()[0]->GetRootNode();
    TR_ASSERT(t, rootNode->GetNodePath() == canonicalRoot);
    TR_ASSERT(t, rootNode->FindNodeWithPath(canonicalRoot) == rootNode);
    TR_ASSERT(t, rootNode->FindNodeWithPath(canonicalRoot / "a") != nullptr);

    // Lookups with the non-canonical form still find the (canonical) node - no duplicate under 'default'
    auto leaf = rootNode->FindNodeWithPath(canonicalRoot / "a" / "leaf.txt");
    TR_ASSERT(t, leaf != nullptr);
    TR_ASSERT(t, workspace.NewDocumentWithFileRef(root / "." / "a" / "leaf.txt") == leaf);

    fs::remove_all(root, ec);
    return kTR_Pass;
}

// Files/folders deleted behind our back (what the folder monitor reports): a node without an open document
// leaves the tree, a node holding an OPEN document stays - flagged, document still open - and saving it
// re-creates the file and clears the flags. Never a crash for a path that isn't in the tree.
DLL_EXPORT int test_workspace_gonefromdisk(ITesting *t) {
    namespace fs = std::filesystem;
    auto root = pathutil::CanonicalPath(fs::temp_directory_path() / "goat_ws_gone");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "a");
    fs::create_directories(root / "b");
    { std::ofstream(root / "a" / "open.txt") << "keep me\n"; }
    { std::ofstream(root / "a" / "closed.txt") << "x\n"; }
    { std::ofstream(root / "b" / "x.txt") << "x\n"; }

    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder(root.string()));
    auto rootNode = workspace.GetProjectRoots()[0]->GetRootNode();
    auto openNode = workspace.NewDocumentWithFileRef(root / "a" / "open.txt");
    auto closedNode = workspace.NewDocumentWithFileRef(root / "a" / "closed.txt");
    TR_ASSERT(t, (openNode != nullptr) && (closedNode != nullptr));
    TR_ASSERT(t, openNode->LoadData());
    workspace.AddOpenDocument(openNode->GetDocument());
    auto nodeA = rootNode->FindNodeWithPath(root / "a");
    auto nodeB = rootNode->FindNodeWithPath(root / "b");
    TR_ASSERT(t, (nodeA != nullptr) && (nodeB != nullptr));

    fs::remove_all(root / "a");
    fs::remove_all(root / "b");

    // Not in the tree (excluded / unscanned / already gone)
    TR_ASSERT(t, !workspace.RemoveNodeGoneFromDisk(nullptr));

    // Nothing open below 'b' - it goes
    TR_ASSERT(t, workspace.RemoveNodeGoneFromDisk(nodeB));
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "b") == nullptr);

    // 'a' holds an open document - 'a' and the open file stay (flagged), the closed file goes
    TR_ASSERT(t, !workspace.RemoveNodeGoneFromDisk(nodeA));
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "a") == nodeA);
    TR_ASSERT(t, nodeA->IsMissingOnDisk());
    TR_ASSERT(t, openNode->GetParent() == nodeA);
    TR_ASSERT(t, nodeA->HasChild(openNode));
    TR_ASSERT(t, openNode->IsMissingOnDisk());
    TR_ASSERT(t, !nodeA->HasChild(closedNode));
    TR_ASSERT(t, workspace.IsDocumentOpen(openNode->GetDocument()));
    TR_ASSERT(t, !rootNode->IsMissingOnDisk());

    // Autosave never brings it back - not even with edits; the edits stay in the buffer
    openNode->GetDocument()->GetTextBuffer()->AddLineUTF8("edited");
    TR_ASSERT(t, !openNode->AutoSaveData());
    TR_ASSERT(t, !fs::exists(root / "a" / "open.txt"));
    TR_ASSERT(t, openNode->IsMissingOnDisk());

    // An explicit save re-creates the file and its folder, and clears the flags
    TR_ASSERT(t, openNode->SaveData());
    TR_ASSERT(t, fs::exists(root / "a" / "open.txt"));
    {
        std::ifstream savedFile(root / "a" / "open.txt");
        std::string savedText((std::istreambuf_iterator<char>(savedFile)), std::istreambuf_iterator<char>());
        TR_ASSERT(t, savedText.find("edited") != std::string::npos);     // the kept edits
    }
    TR_ASSERT(t, !openNode->IsMissingOnDisk());
    TR_ASSERT(t, !nodeA->IsMissingOnDisk());
    // Back on disk - autosave works as usual again
    TR_ASSERT(t, openNode->AutoSaveData());

    fs::remove_all(root, ec);
    return kTR_Pass;
}

// A monitor point without an OS backend - the test plays the OS thread through OnRawEvent/OnLostEvents
class FakeMonitorPoint : public FolderMonitor::MonitorPoint {
public:
    using Ref = std::shared_ptr<FakeMonitorPoint>;
public:
    explicit FakeMonitorPoint(const std::filesystem::path &path) : FolderMonitor::MonitorPoint(path) {

    }
    bool Start() override {
        isRunning = true;
        return true;
    }
    bool Stop() override {
        isRunning = false;
        return true;
    }
    void Event(const std::filesystem::path &path) {
        OnRawEvent(path);
    }
    void LostEvents() {
        OnLostEvents();
    }
};

// Wires 'workspace' to fake monitors; posted flushes queue up until the test runs them
struct FakeMonitorGlue {
    FakeMonitorPoint::Ref monitor = nullptr;
    std::vector<std::function<void()>> posted = {};

    void Wire(Workspace &workspace) {
        workspace.SetFolderMonitor([this](const std::filesystem::path &rootPath) -> FolderMonitor::MonitorPoint::Ref {
            monitor = std::make_shared<FakeMonitorPoint>(rootPath);
            return monitor;
        }, [this](std::function<void()> callback) {
            posted.push_back(callback);
        });
    }
    void RunPosted() {
        auto toRun = std::move(posted);
        posted.clear();
        for (auto &callback : toRun) {
            callback();
        }
    }
};

// Folder-monitor events below an excluded folder (workspace.exclude, any path component) or outside the
// root never reach the tree - a rebuild in cmake-build-debug used to add the folder / spam errors.
DLL_EXPORT int test_workspace_monitorexclude(ITesting *t) {
    namespace fs = std::filesystem;
    auto root = pathutil::CanonicalPath(fs::temp_directory_path() / "goat_ws_monexclude");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "src");
    fs::create_directories(root / "cmake-build-debug" / "CMakeFiles");

    auto oldExclude = Config::Instance()["workspace"].GetSequenceOfStr("exclude");
    auto cfgWorkspace = Config::Instance()["workspace"];
    cfgWorkspace.SetValue<std::vector<std::string>>("exclude", {".git", "cmake-build-*"});

    Workspace workspace;
    FakeMonitorGlue glue;
    glue.Wire(workspace);
    TR_ASSERT(t, workspace.OpenFolder(root.string()));
    TR_ASSERT(t, (glue.monitor != nullptr) && glue.monitor->IsRunning());
    auto monitor = glue.monitor;

    TR_ASSERT(t, monitor->IsPathExcluded(root / "cmake-build-debug"));
    TR_ASSERT(t, monitor->IsPathExcluded(root / "cmake-build-debug" / "CMakeFiles" / "y.o"));
    TR_ASSERT(t, monitor->IsPathExcluded(root / "src" / ".git" / "HEAD"));
    TR_ASSERT(t, monitor->IsPathExcluded(root.parent_path() / "elsewhere.txt"));

    TR_ASSERT(t, !monitor->IsPathExcluded(root));
    TR_ASSERT(t, !monitor->IsPathExcluded(root / "src"));
    TR_ASSERT(t, !monitor->IsPathExcluded(root / "src" / "main.cpp"));
    TR_ASSERT(t, !monitor->IsPathExcluded(root / ".gitignore"));

    // A rebuild: nothing is posted at all
    { std::ofstream(root / "cmake-build-debug" / "CMakeFiles" / "y.o") << "x\n"; }
    monitor->Event(root / "cmake-build-debug" / "CMakeFiles" / "y.o");
    monitor->Event(root / "cmake-build-debug");
    monitor->Event(root.parent_path() / "elsewhere.txt");
    TR_ASSERT(t, glue.posted.empty());

    cfgWorkspace.SetValue<std::vector<std::string>>("exclude", oldExclude);
    fs::remove_all(root, ec);
    return kTR_Pass;
}

// (Re)scanning a folder keeps the nodes already there - an open document's node (and its Document) must
// survive. The old ScanNode cleared the children and rebuilt them, detaching the open document.
DLL_EXPORT int test_workspace_scannode_keepsopen(ITesting *t) {
    namespace fs = std::filesystem;
    auto root = pathutil::CanonicalPath(fs::temp_directory_path() / "goat_ws_scankeep");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "a");
    { std::ofstream(root / "a" / "open.txt") << "x\n"; }

    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder(root.string()));
    auto rootNode = workspace.GetProjectRoots()[0]->GetRootNode();
    auto openNode = workspace.NewDocumentWithFileRef(root / "a" / "open.txt");
    TR_ASSERT(t, openNode != nullptr);
    workspace.AddOpenDocument(openNode->GetDocument());
    auto nodeA = openNode->GetParent();

    workspace.ScanNode(nodeA);
    TR_ASSERT(t, nodeA->HasChild(openNode));
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "a" / "open.txt") == openNode);
    TR_ASSERT(t, workspace.GetNodeFromDocument(openNode->GetDocument()) == openNode);

    fs::remove_all(root, ec);
    return kTR_Pass;
}

// ReconcileFolder - the folder monitor's "something changed in this folder": one level, against disk.
// Renames are just 'old gone + new there'; nodes still on disk are kept as they are.
DLL_EXPORT int test_workspace_reconcile(ITesting *t) {
    namespace fs = std::filesystem;
    auto root = pathutil::CanonicalPath(fs::temp_directory_path() / "goat_ws_reconcile");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "a");
    fs::create_directories(root / "sub" / "deep");
    { std::ofstream(root / "a" / "open.txt") << "x\n"; }
    { std::ofstream(root / "a" / "closed.txt") << "x\n"; }
    { std::ofstream(root / "keep.txt") << "x\n"; }
    { std::ofstream(root / "old.txt") << "x\n"; }

    auto oldExclude = Config::Instance()["workspace"].GetSequenceOfStr("exclude");
    auto cfgWorkspace = Config::Instance()["workspace"];
    cfgWorkspace.SetValue<std::vector<std::string>>("exclude", {".git", "cmake-build-*"});

    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder(root.string()));
    auto rootNode = workspace.GetProjectRoots()[0]->GetRootNode();
    auto keep = rootNode->FindNodeWithPath(root / "keep.txt");
    auto nodeA = rootNode->FindNodeWithPath(root / "a");
    auto openNode = workspace.NewDocumentWithFileRef(root / "a" / "open.txt");
    workspace.AddOpenDocument(openNode->GetDocument());
    // A '.nb' buffer, never saved - open, not on disk
    workspace.SetActiveFolderNode(rootNode);
    auto ghost = workspace.NewDocument("ghost.cpp");
    TR_ASSERT(t, (ghost != nullptr) && ghost->IsNewFile());
    workspace.AddOpenDocument(ghost->GetDocument());

    // Nothing changed on disk
    TR_ASSERT(t, !workspace.ReconcileFolder(rootNode));

    fs::rename(root / "old.txt", root / "new.txt");
    fs::create_directories(root / "newdir");
    { std::ofstream(root / "newdir" / "inside.txt") << "x\n"; }
    fs::create_directories(root / "cmake-build-debug");
    fs::remove_all(root / "sub");

    TR_ASSERT(t, workspace.ReconcileFolder(rootNode));
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "old.txt") == nullptr);
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "new.txt") != nullptr);
    auto newDir = rootNode->FindNodeWithPath(root / "newdir");
    TR_ASSERT(t, (newDir != nullptr) && newDir->IsFolder() && !newDir->IsScanned());
    TR_ASSERT(t, newDir->GetNumChildNodes() == 0);                     // a frontier - filled on expand
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "cmake-build-debug") == nullptr);
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "sub") == nullptr);
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "keep.txt") == keep);  // untouched - same node
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "a") == nodeA);
    TR_ASSERT(t, rootNode->HasChild(ghost) && !ghost->IsMissingOnDisk());

    // Expanding the new folder fills it
    workspace.ScanNode(newDir);
    TR_ASSERT(t, newDir->IsScanned() && (newDir->GetNumChildNodes() == 1));

    // Files gone below 'a': the open one is kept and flagged, the closed one goes
    fs::remove(root / "a" / "open.txt");
    fs::remove(root / "a" / "closed.txt");
    TR_ASSERT(t, workspace.ReconcileFolder(nodeA));
    TR_ASSERT(t, nodeA->HasChild(openNode) && openNode->IsMissingOnDisk());
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "a" / "closed.txt") == nullptr);
    TR_ASSERT(t, !workspace.ReconcileFolder(nodeA));      // stable - nothing new

    // The file comes back (e.g. 'git checkout') - same node, flag cleared
    { std::ofstream(root / "a" / "open.txt") << "x\n"; }
    TR_ASSERT(t, workspace.ReconcileFolder(nodeA));
    TR_ASSERT(t, nodeA->HasChild(openNode) && !openNode->IsMissingOnDisk());

    // The whole folder goes - kept (open document below), still a folder
    fs::remove_all(root / "a");
    TR_ASSERT(t, workspace.ReconcileFolder(nodeA));
    TR_ASSERT(t, rootNode->HasChild(nodeA) && nodeA->IsMissingOnDisk() && nodeA->IsFolder());
    TR_ASSERT(t, openNode->IsMissingOnDisk());

    // Saving the ghost writes it - no longer a new file
    ghost->GetDocument()->GetTextBuffer()->AddLineUTF8("int x;");
    TR_ASSERT(t, ghost->SaveData());
    TR_ASSERT(t, fs::exists(root / "ghost.cpp") && !ghost->IsNewFile());

    cfgWorkspace.SetValue<std::vector<std::string>>("exclude", oldExclude);
    fs::remove_all(root, ec);
    return kTR_Pass;
}

// A burst of monitor events is ONE flush on the main thread: the dirty folders are reconciled and the tree
// is notified once. Unscanned frontiers are left for the expand; lost events reconcile everything loaded.
DLL_EXPORT int test_workspace_monitorflush(ITesting *t) {
    namespace fs = std::filesystem;
    auto root = pathutil::CanonicalPath(fs::temp_directory_path() / "goat_ws_monflush");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "src");
    fs::create_directories(root / "sub");
    { std::ofstream(root / "src" / "a.txt") << "x\n"; }

    Workspace workspace;
    FakeMonitorGlue glue;
    glue.Wire(workspace);
    int nNotify = 0;
    workspace.SetChangeDelegate([&nNotify]() { nNotify++; });
    TR_ASSERT(t, workspace.OpenFolder(root.string()));
    TR_ASSERT(t, glue.monitor != nullptr);
    auto monitor = glue.monitor;
    auto rootNode = workspace.GetProjectRoots()[0]->GetRootNode();
    auto srcNode = rootNode->FindNodeWithPath(root / "src");
    TR_ASSERT(t, srcNode != nullptr);

    // Three changes in one folder -> one post, one notification
    nNotify = 0;
    { std::ofstream(root / "src" / "b.txt") << "x\n"; }
    { std::ofstream(root / "src" / "c.txt") << "x\n"; }
    fs::remove(root / "src" / "a.txt");
    monitor->Event(root / "src" / "b.txt");
    monitor->Event(root / "src" / "c.txt");
    monitor->Event(root / "src" / "a.txt");
    TR_ASSERT(t, glue.posted.size() == 1);
    TR_ASSERT(t, nNotify == 0);           // nothing happens until the main thread runs the flush
    glue.RunPosted();
    TR_ASSERT(t, nNotify == 1);
    TR_ASSERT(t, srcNode->GetNumChildNodes() == 2);
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "src" / "a.txt") == nullptr);
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "src" / "b.txt") != nullptr);

    // An event that changes nothing in the tree (a file written) -> flushed, but no notification
    { std::ofstream(root / "src" / "b.txt") << "more\n"; }
    monitor->Event(root / "src" / "b.txt");
    TR_ASSERT(t, glue.posted.size() == 1);
    glue.RunPosted();
    TR_ASSERT(t, nNotify == 1);

    // A new folder arrives as a frontier; what is written inside it waits for the expand
    fs::create_directories(root / "fresh");
    monitor->Event(root / "fresh");
    glue.RunPosted();
    TR_ASSERT(t, nNotify == 2);
    auto freshNode = rootNode->FindNodeWithPath(root / "fresh");
    TR_ASSERT(t, (freshNode != nullptr) && !freshNode->IsScanned());
    { std::ofstream(root / "fresh" / "inner.txt") << "x\n"; }
    monitor->Event(root / "fresh" / "inner.txt");
    glue.RunPosted();
    TR_ASSERT(t, nNotify == 2);
    TR_ASSERT(t, freshNode->GetNumChildNodes() == 0);

    // The OS dropped events: changes nobody reported are picked up in every scanned folder
    { std::ofstream(root / "sub" / "unseen.txt") << "x\n"; }
    fs::remove(root / "src" / "c.txt");
    monitor->LostEvents();
    TR_ASSERT(t, glue.posted.size() == 1);
    glue.RunPosted();
    TR_ASSERT(t, nNotify == 3);
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "sub" / "unseen.txt") != nullptr);
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "src" / "c.txt") == nullptr);
    TR_ASSERT(t, freshNode->GetNumChildNodes() == 0);     // still a frontier

    // A whole folder removed - the parent's reconcile drops it, its own (now dangling) dirty entry is skipped
    fs::remove_all(root / "src");
    monitor->Event(root / "src" / "b.txt");
    monitor->Event(root / "src");
    glue.RunPosted();
    TR_ASSERT(t, nNotify == 4);
    TR_ASSERT(t, rootNode->FindNodeWithPath(root / "src") == nullptr);

    fs::remove_all(root, ec);
    return kTR_Pass;
}

// A flush queued on the main thread can outlive its workspace (folder closed / editor shutting down)
DLL_EXPORT int test_workspace_monitorlifetime(ITesting *t) {
    namespace fs = std::filesystem;
    auto root = pathutil::CanonicalPath(fs::temp_directory_path() / "goat_ws_monlife");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);

    // The ProjectRoot is gone, the monitor point is still alive (held here)
    FakeMonitorGlue glue;
    {
        auto workspace = Workspace::Create();
        glue.Wire(*workspace);
        TR_ASSERT(t, workspace->OpenFolder(root.string()));
        { std::ofstream(root / "x.txt") << "x\n"; }
        glue.monitor->Event(root / "x.txt");
        TR_ASSERT(t, glue.posted.size() == 1);
    }
    TR_ASSERT(t, !glue.monitor->IsRunning());     // the ProjectRoot stopped it
    glue.RunPosted();

    // Both gone
    {
        auto workspace = Workspace::Create();
        glue.Wire(*workspace);
        TR_ASSERT(t, workspace->OpenFolder(root.string()));
        { std::ofstream(root / "y.txt") << "x\n"; }
        glue.monitor->Event(root / "y.txt");
        TR_ASSERT(t, glue.posted.size() == 1);
    }
    glue.monitor = nullptr;
    glue.RunPosted();

    fs::remove_all(root, ec);
    return kTR_Pass;
}

// Buffer N (Cmd/Alt+N): 1..8 is that tab, 9 is the last one (browser/terminal convention); a tab that
// doesn't exist is no jump.
DLL_EXPORT int test_workspace_tabnumber(ITesting *t) {
    Workspace workspace;
    TR_ASSERT(t, !workspace.DocumentIndexForTabNumber(1).has_value());   // nothing open
    TR_ASSERT(t, !workspace.DocumentIndexForTabNumber(9).has_value());

    for (int i = 0; i < 3; i++) {
        auto node = workspace.NewDocument("tab" + std::to_string(i) + ".txt");
        workspace.AddOpenDocument(node->GetDocument());
    }
    TR_ASSERT(t, workspace.DocumentIndexForTabNumber(1) == 0);
    TR_ASSERT(t, workspace.DocumentIndexForTabNumber(3) == 2);
    TR_ASSERT(t, !workspace.DocumentIndexForTabNumber(4).has_value());
    TR_ASSERT(t, !workspace.DocumentIndexForTabNumber(8).has_value());
    TR_ASSERT(t, workspace.DocumentIndexForTabNumber(9) == 2);           // the last
    TR_ASSERT(t, !workspace.DocumentIndexForTabNumber(0).has_value());
    TR_ASSERT(t, !workspace.DocumentIndexForTabNumber(10).has_value());
    return kTR_Pass;
}

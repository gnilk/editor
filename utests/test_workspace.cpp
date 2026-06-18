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
#include "Core/Editor.h"
#include "Core/Workspace.h"
#include "Core/RuntimeConfig.h"

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
DLL_EXPORT int test_workspace_removedocument(ITesting *t);
DLL_EXPORT int test_workspace_recreate(ITesting *t);
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
DLL_EXPORT int test_workspace_openfolder_excludes_builddir(ITesting *t) {
    namespace fs = std::filesystem;
    static int counter = 0;
    auto root = fs::temp_directory_path() / ("goat_ws_exclude_" + std::to_string(counter++));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "src");
    fs::create_directories(root / "cmake-build-debug" / "_deps");
    { std::ofstream(root / "src" / "main.cpp") << "x"; }
    { std::ofstream(root / "cmake-build-debug" / "build.ninja") << "x"; }
    { std::ofstream(root / "cmake-build-debug" / "_deps" / "dep.c") << "x"; }

    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder(root.string()));

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

// FS-6 W2: Node::isScanned is written by the onLeaveDir adapter. With a low maxScanDepth,
// frontier folder nodes (emitted but not descended) carry isScanned==false; a descended-and-empty
// folder stays isScanned==true (the walked-empty case — expanding it is a known no-op, not a retry).
DLL_EXPORT int test_workspace_isscanned(ITesting *t) {
    namespace fs = std::filesystem;
    static int counter = 0;
    auto root = fs::temp_directory_path() / ("goat_ws_isscanned_" + std::to_string(counter++));
    std::error_code ec;
    fs::remove_all(root, ec);
    // a/b/deep.txt: "a" at depth 0 gets descended (isScanned=true), "b" at depth 1 hits the bound
    // (depth+1=2 >= maxDepth=2 → isScanned=false). "empty" at depth 0 is descended and empty → true.
    fs::create_directories(root / "a" / "b");
    fs::create_directories(root / "empty");
    { std::ofstream(root / "a" / "b" / "deep.txt") << "x"; }

    auto oldDepth = Config::Instance()["workspace"].GetInt("maxScanDepth", 64);
    Config::Instance()["workspace"].SetInt("maxScanDepth", 2);

    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder(root.string()));

    Config::Instance()["workspace"].SetInt("maxScanDepth", oldDepth);

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

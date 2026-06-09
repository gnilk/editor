//
// Created by gnilk on 09.05.23.
//
#include <testinterface.h>
#include <chrono>
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
DLL_EXPORT int test_workspace_newmodel(ITesting *t);
DLL_EXPORT int test_workspace_openfolder(ITesting *t);
DLL_EXPORT int test_workspace_openfolder_lazy(ITesting *t);
DLL_EXPORT int test_workspace_openabsfolder(ITesting *t);
DLL_EXPORT int test_workspace_removemodel(ITesting *t);
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
    auto model = workspace.NewDocument("dummy");
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

    auto models = workspace.GetDefaultWorkspace()->GetDocuments();
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

DLL_EXPORT int test_workspace_newmodel(ITesting *t) {
    Workspace workspace;
    auto node = workspace.NewDocument("wef");

    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetDocuments().size() != 0);
    TR_ASSERT(t, node->GetTextBuffer()->GetBufferState() == TextBuffer::BufferState::kBuffer_FileRef);

    return kTR_Pass;
}

DLL_EXPORT int test_workspace_fileref(ITesting *t) {
    Workspace workspace;
    std::filesystem::path filename("testfiles/ConvertUTF.cpp");
    auto model = workspace.NewDocumentWithFileRef(filename);

    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetDocuments().size() != 0);
    TR_ASSERT(t, model->GetTextBuffer()->GetBufferState() == TextBuffer::BufferState::kBuffer_FileRef);
    TR_ASSERT(t, model->LoadData());
    TR_ASSERT(t, model->GetTextBuffer()->GetBufferState() == TextBuffer::BufferState::kBuffer_Loaded);

    return kTR_Pass;
}

DLL_EXPORT int test_workspace_openfolder(ITesting *t) {
    Workspace workspace;
    TR_ASSERT(t, workspace.OpenFolder("."));
    auto desktops = workspace.GetProjectRoots();
    TR_ASSERT(t, desktops.size() == 1);
    auto desktop = desktops[0];
    auto rootNode = desktop->GetRootNode();
    // Lazy tree: scanning a folder builds path-only nodes - NO models exist until a node is opened.
    TR_ASSERT(t, rootNode->GetNumChildNodes() > 0);
    TR_ASSERT(t, rootNode->GetDocuments().size() == 0);
    return kTR_Pass;
}

// Opening a scanned (path-only) file node lazily creates exactly one model, and re-opening reuses it.
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

    auto model = workspace.EnsureDocumentForNode(fileNode);
    TR_ASSERT(t, model != nullptr);
    TR_ASSERT(t, fileNode->GetDocument() == model);
    // The model adopts the node's path as its identity
    TR_ASSERT(t, model->GetPath() == fileNode->GetNodePath());
    // Re-ensuring reuses the same model (does not rebuild)
    TR_ASSERT(t, workspace.EnsureDocumentForNode(fileNode) == model);

    return kTR_Pass;
}

DLL_EXPORT int test_workspace_openabsfolder(ITesting *t) {
    Workspace workspace;
#ifdef GEDIT_LINUX
    workspace.OpenFolder("/home/gnilk/src/test/sdl2test");
#endif
    return kTR_Pass;

}


DLL_EXPORT int test_workspace_removemodel(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", true);

    Workspace workspace;
    {
        auto node = workspace.NewDocument("wef");
        workspace.RemoveDocument(node->GetDocument());
    } // should lose the shared_ptr for the model when leaving this block...

    return kTR_Pass;
}

DLL_EXPORT int test_workspace_recreate(ITesting *t) {
    Workspace::Ref workspace = Workspace::Create();
    // Create a number of models
    workspace->NewDocument("m1");
    workspace->NewDocument("m2");
    workspace->NewDocument("m2");

    // Let's see if all DTOR's are invoked
    // note: in order to test this - set breakpoints in DTORs
    workspace = nullptr;
    std::this_thread::sleep_for(500ms);
    return kTR_Pass;


}

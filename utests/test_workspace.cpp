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
DLL_EXPORT int test_workspace_openabsfolder(ITesting *t);
DLL_EXPORT int test_workspace_removemodel(ITesting *t);
DLL_EXPORT int test_workspace_recreate(ITesting *t);
}

DLL_EXPORT int test_workspace(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_workspace_empty(ITesting *t) {
    Workspace workspace;
    TR_ASSERT(t, workspace.GetDesktops().size() == 0);
    return kTR_Pass;
}
DLL_EXPORT int test_workspace_new(ITesting *t) {
    Workspace workspace;
    TR_ASSERT(t, workspace.GetDesktops().size() == 0);
    auto model = workspace.NewModel("dummy");
    TR_ASSERT(t, workspace.GetDesktops().size() != 0);
    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetModels().size() != 0);
    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetModels().size() == 1);
    for(auto &m : workspace.GetDefaultWorkspace()->GetModels()) {
        auto node = workspace.GetNodeFromModel(m);

        TR_ASSERT(t, node->GetDisplayName() == "dummy");
    }

    return kTR_Pass;
}

DLL_EXPORT int test_workspace_newtwice(ITesting *t) {
    Workspace workspace;
    TR_ASSERT(t, workspace.GetDesktops().size() == 0);
    workspace.NewModel("m1");
    workspace.NewModel("m2");
    TR_ASSERT(t, workspace.GetDesktops().size() != 0);
    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetModels().size() != 0);
    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetModels().size() == 2);

    auto models = workspace.GetDefaultWorkspace()->GetModels();
    int count = 0;
    // We can't really guarantee the return order there
    for(auto &m : workspace.GetDefaultWorkspace()->GetModels()) {
        auto node = workspace.GetNodeFromModel(m);
        char buffer[64];
        snprintf(buffer,63,"new_%d", count);
        printf("Name: %s == %s\n", buffer,node->GetDisplayName().c_str());
        count++;
    }
    return kTR_Pass;
}

DLL_EXPORT int test_workspace_newmodel(ITesting *t) {
    Workspace workspace;
    auto node = workspace.NewModel("wef");

    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetModels().size() != 0);
    TR_ASSERT(t, node->GetTextBuffer()->GetBufferState() == TextBuffer::BufferState::kBuffer_Empty);

    return kTR_Pass;
}

DLL_EXPORT int test_workspace_fileref(ITesting *t) {
    Workspace workspace;
    std::filesystem::path filename("./test_src2.cpp");
    auto model = workspace.NewModelWithFileRef(filename);

    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetModels().size() != 0);
    TR_ASSERT(t, model->GetTextBuffer()->GetBufferState() == TextBuffer::BufferState::kBuffer_FileRef);
    TR_ASSERT(t, model->LoadData());
    TR_ASSERT(t, model->GetTextBuffer()->GetBufferState() == TextBuffer::BufferState::kBuffer_Loaded);

    return kTR_Pass;
}

DLL_EXPORT int test_workspace_openfolder(ITesting *t) {
    Workspace workspace;
//    TR_ASSERT(t, workspace.GetModels().size() == 0);
    workspace.OpenFolder(".");
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
        auto node = workspace.NewModel("wef");
        workspace.RemoveModel(node->GetModel());
    } // should lose the shared_ptr for the model when leaving this block...

    return kTR_Pass;
}

DLL_EXPORT int test_workspace_recreate(ITesting *t) {
    Workspace::Ref workspace = Workspace::Create();
    // Create a number of models
    workspace->NewModel("m1");
    workspace->NewModel("m2");
    workspace->NewModel("m2");

    // Let's see if all DTOR's are invoked
    // note: in order to test this - set breakpoints in DTORs
    workspace = nullptr;
    std::this_thread::sleep_for(500ms);
    return kTR_Pass;


}

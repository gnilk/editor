//
// Created by gnilk on 09.05.23.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/Workspace.h"
#include "Core/RuntimeConfig.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_workspace(ITesting *t);
DLL_EXPORT int test_workspace_empty(ITesting *t);
DLL_EXPORT int test_workspace_fileref(ITesting *t);
DLL_EXPORT int test_workspace_newmodel(ITesting *t);
DLL_EXPORT int test_workspace_openfolder(ITesting *t);
}

DLL_EXPORT int test_workspace(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_workspace_empty(ITesting *t) {
    Workspace workspace;
    TR_ASSERT(t, workspace.GetRootNodes().size() != 0);
    TR_ASSERT(t, workspace.GetModels().size() == 0);
    return kTR_Pass;
}

DLL_EXPORT int test_workspace_newmodel(ITesting *t) {
    Workspace workspace;
    auto model = workspace.NewEmptyModel();

    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetModels().size() != 0);
    TR_ASSERT(t, model->GetTextBuffer()->GetBufferState() == TextBuffer::BufferState::kBuffer_Empty);

    return kTR_Pass;
}

DLL_EXPORT int test_workspace_fileref(ITesting *t) {
    Workspace workspace;
    std::filesystem::path filename("./colors.json");
    auto model = workspace.NewModelWithFileRef(filename);

    TR_ASSERT(t, workspace.GetDefaultWorkspace()->GetModels().size() != 0);
    TR_ASSERT(t, model->GetTextBuffer()->GetBufferState() == TextBuffer::BufferState::kBuffer_FileRef);
    TR_ASSERT(t, model->GetTextBuffer()->Load());
    TR_ASSERT(t, model->GetTextBuffer()->GetBufferState() == TextBuffer::BufferState::kBuffer_Loaded);

    return kTR_Pass;
}



DLL_EXPORT int test_workspace_openfolder(ITesting *t) {
    Workspace workspace;
    TR_ASSERT(t, workspace.GetModels().size() == 0);
    workspace.OpenFolder(".");
    return kTR_Pass;
}


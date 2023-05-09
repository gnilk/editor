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
}

DLL_EXPORT int test_workspace(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_workspace_empty(ITesting *t) {
    Workspace workspace;
    TR_ASSERT(t, workspace.GetModels().size() == 0);
    workspace.GetActiveModelIndex();
    return kTR_Pass;
}


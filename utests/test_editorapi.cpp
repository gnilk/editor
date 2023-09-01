//
// Created by gnilk on 05.05.23.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/API/EditorAPI.h"


using namespace gedit;

extern "C" {
DLL_EXPORT int test_editorapi(ITesting *t);
DLL_EXPORT int test_editorapi_listlang(ITesting *t);
DLL_EXPORT int test_editorapi_newbuffer(ITesting *t);
DLL_EXPORT int test_editorapi_loadbuffer(ITesting *t);
}
DLL_EXPORT int test_editorapi(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_editorapi_listlang(ITesting *t) {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    auto v = editorApi->GetRegisteredLanguages();
    TR_ASSERT(t, v.size() != 0);
    for(auto &l : v) {
        printf("%s\n",l.c_str());
    }
    return kTR_Pass;
}

DLL_EXPORT int test_editorapi_newbuffer(ITesting *t) {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    auto numBefore = Editor::Instance().GetModels().size();
    editorApi->NewDocument("mamma");
    auto numAfter = Editor::Instance().GetModels().size();
    TR_ASSERT(t, numAfter > numBefore);

    return kTR_Pass;
}

DLL_EXPORT int test_editorapi_loadbuffer(ITesting *t) {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    auto numBefore = Editor::Instance().GetModels().size();
    editorApi->NewDocument("example.json");
    auto numAfter = Editor::Instance().GetModels().size();
    TR_ASSERT(t, numAfter > numBefore);
    return kTR_Pass;
}

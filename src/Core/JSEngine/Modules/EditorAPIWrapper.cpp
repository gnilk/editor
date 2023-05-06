#include "EditorAPIWrapper.h"
#include "Core/API/EditorAPI.h"
#include "dukglue/dukglue.h"

using namespace gedit;

void EditorAPIWrapper::RegisterModule(duk_context *ctx) {
//    dukglue_register_constructor<EditorAPIWrapper>(ctx, "Editor");
    static EditorAPIWrapper editorApiWrapper;

    dukglue_push(ctx, &editorApiWrapper);
    duk_put_global_string(ctx, "Editor");

    dukglue_register_method(ctx, &EditorAPIWrapper::GetActiveTextBuffer, "GetActiveTextBuffer");
    dukglue_register_method(ctx, &EditorAPIWrapper::ExitEditor, "ExitEditor");
    dukglue_register_method(ctx, &EditorAPIWrapper::GetRegisteredLanguages, "GetRegisteredLanguages");
    dukglue_register_method(ctx, &EditorAPIWrapper::NewBuffer, "NewBuffer");
    dukglue_register_method(ctx, &EditorAPIWrapper::LoadBuffer, "LoadBuffer");
    dukglue_register_method(ctx, &EditorAPIWrapper::SetActiveBuffer, "SetActiveBuffer");

    // Some test stuff...
    dukglue_register_method(ctx, &EditorAPIWrapper::GetTestArray, "GetTestArray");

}

//
// Impl API
//
TextBufferAPIWrapper::Ref EditorAPIWrapper::GetActiveTextBuffer() {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    return TextBufferAPIWrapper::Create(editorApi->GetActiveTextBuffer());
}

void EditorAPIWrapper::ExitEditor() {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    editorApi->ExitEditor();
}
std::vector<std::string> EditorAPIWrapper::GetRegisteredLanguages() {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    return editorApi->GetRegisteredLanguages();
}

void EditorAPIWrapper::NewBuffer(const char *name) {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    editorApi->NewBuffer(name);
}
TextBufferAPI::Ref EditorAPIWrapper::LoadBuffer(const char *name) {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    return editorApi->LoadBuffer(name);
}
void EditorAPIWrapper::SetActiveBuffer(TextBufferAPI::Ref activeBuffer) {
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    editorApi->SetActiveBuffer(activeBuffer);
}

//
// Test stuff
//
std::vector<std::string> EditorAPIWrapper::GetTestArray() {
    static std::vector<std::string> v = {"1","5","21","4"};
    return v;
}





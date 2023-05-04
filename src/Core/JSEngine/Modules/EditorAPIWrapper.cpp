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
}

//
// Impl API
//

TextBufferAPIWrapper::Ref EditorAPIWrapper::GetActiveTextBuffer() {
    printf("GetActiveTextBuffer: %p\n", this);
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    return TextBufferAPIWrapper::Create(editorApi->GetActiveTextBuffer());
}






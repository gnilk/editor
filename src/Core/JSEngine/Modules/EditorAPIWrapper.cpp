#include "EditorAPIWrapper.h"
#include "dukglue/dukglue.h"

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

TextBufferAPIWrapper *EditorAPIWrapper::GetActiveTextBuffer() {
    printf("GetActiveTextBuffer: %p\n", this);
    static TextBufferAPIWrapper dummy;
    return &dummy;
}






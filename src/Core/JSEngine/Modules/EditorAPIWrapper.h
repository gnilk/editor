#ifndef GEDIT_EDITORWRAPPERAPI_H
#define GEDIT_EDITORWRAPPERAPI_H

#include "duktape.h"

#include "TextBufferAPIWrapper.h"

namespace gedit {
    class EditorAPIWrapper {
    public:
        TextBufferAPIWrapper *GetActiveTextBuffer();
        static void RegisterModule(duk_context *ctx);
    public:
    };
}

#endif
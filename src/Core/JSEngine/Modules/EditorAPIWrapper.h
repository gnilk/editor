#ifndef GEDIT_EDITORWRAPPERAPI_H
#define GEDIT_EDITORWRAPPERAPI_H

#include "duktape.h"
#include "TextBufferAPIWrapper.h"

namespace gedit {
    class EditorAPIWrapper {
    public:
        TextBufferAPIWrapper::Ref GetActiveTextBuffer();
        static void RegisterModule(duk_context *ctx);
        // For testing purposes
        std::vector<std::string> GetTestArray();

    public:
    };
}

#endif
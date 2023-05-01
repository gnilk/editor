
#ifndef GEDIT_TEXTBUFFERAPIWRAPPER_H
#define GEDIT_TEXTBUFFERAPIWRAPPER_H

#include "Core/TextBuffer.h"
#include "duktape.h"

namespace gedit {
    class TextBufferAPIWrapper {
    public:
        TextBufferAPIWrapper() = default;
        TextBufferAPIWrapper(TextBuffer::Ref tBuffer) : textBuffer(tBuffer) {

        }
        void SetLanguage(const char *param );
        static void RegisterModule(duk_context *ctx);
    private:
        TextBuffer::Ref textBuffer = nullptr;
    };
}

#endif
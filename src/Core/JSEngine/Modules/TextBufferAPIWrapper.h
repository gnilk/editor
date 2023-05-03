
#ifndef GEDIT_TEXTBUFFERAPIWRAPPER_H
#define GEDIT_TEXTBUFFERAPIWRAPPER_H

#include "Core/API/TextBufferAPI.h"
#include "duktape.h"

namespace gedit {
    class TextBufferAPIWrapper {
    public:
        TextBufferAPIWrapper() = default;
        TextBufferAPIWrapper(TextBufferAPI *tBuffer) : textBuffer(tBuffer) {
        }
        virtual ~TextBufferAPIWrapper() {
            delete textBuffer;
        };
        void SetLanguage(const char *param );
        static void RegisterModule(duk_context *ctx);
    private:
        TextBufferAPI *textBuffer = nullptr;
    };
}

#endif
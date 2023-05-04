
#ifndef GEDIT_TEXTBUFFERAPIWRAPPER_H
#define GEDIT_TEXTBUFFERAPIWRAPPER_H

#include <memory>

#include "duktape.h"

#include "Core/API/TextBufferAPI.h"

namespace gedit {
    class TextBufferAPIWrapper {
    public:
        using Ref = std::shared_ptr<TextBufferAPIWrapper>;
    public:
        TextBufferAPIWrapper() = default;
        TextBufferAPIWrapper(TextBufferAPI::Ref tBuffer) : textBuffer(tBuffer) {
        }
        virtual ~TextBufferAPIWrapper() {
            printf("TextBufferAPIWrapper::DTOR\n");
        };

        static Ref Create(TextBufferAPI::Ref tBuffer) {
            return std::make_shared<TextBufferAPIWrapper>(tBuffer);
        }

        void SetLanguage(const char *param );
        static void RegisterModule(duk_context *ctx);
    private:
        TextBufferAPI::Ref textBuffer = nullptr;
    };
}

#endif
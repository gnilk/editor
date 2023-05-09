//
// Created by gnilk on 03.05.23.
//

#ifndef EDITOR_TEXTBUFFERAPI_H
#define EDITOR_TEXTBUFFERAPI_H

#include <memory>
#include "Core/TextBuffer.h"

namespace gedit {
    class TextBufferAPI {
    public:
        using Ref = std::shared_ptr<TextBufferAPI>;
    public:
        TextBufferAPI() {
            printf("TextBufferAPI::CTOR\n");
        }
        TextBufferAPI(TextBuffer::Ref tBuffer) : textBuffer(tBuffer) {
            printf("TextBufferAPI::CTOR tBuffer\n");
        }
        virtual ~TextBufferAPI() {
            printf("TextBufferAPI::DTOR!\n");
        }
        void SetLanguage(const char *param );

        TextBuffer::Ref GetTextBuffer() {
            return textBuffer;
        }

        const std::string &GetName() {
            return textBuffer->Name();
        }

        bool HasFileName() {
            return textBuffer->HasPathName();
        }
        void SetFileName(const std::string &name) {
            return textBuffer->SetNameFromFileName(name);
        }
        const std::string GetFileName() {
            return textBuffer->GetFileName();
        }
        bool SaveBuffer();

    private:
        TextBuffer::Ref textBuffer = nullptr;

    };
}


#endif //EDITOR_TEXTBUFFERAPI_H

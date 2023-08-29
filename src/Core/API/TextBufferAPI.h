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
        TextBufferAPI() = default;
        TextBufferAPI(TextBuffer::Ref tBuffer) : textBuffer(tBuffer) {
        }
        virtual ~TextBufferAPI() = default;
        void SetLanguage(const char *param );

        TextBuffer::Ref GetTextBuffer() {
            return textBuffer;
        }

    private:
        TextBuffer::Ref textBuffer = nullptr;

    };
}


#endif //EDITOR_TEXTBUFFERAPI_H

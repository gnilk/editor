//
// Created by gnilk on 03.05.23.
//

#ifndef EDITOR_TEXTBUFFERAPI_H
#define EDITOR_TEXTBUFFERAPI_H

#include "Core/TextBuffer.h"

namespace gedit {
    class TextBufferAPI {
    public:
        TextBufferAPI() = default;
        TextBufferAPI(TextBuffer::Ref tBuffer) : textBuffer(tBuffer) {

        }
        virtual ~TextBufferAPI() = default;
        void SetLanguage(const char *param );

    private:
        TextBuffer::Ref textBuffer = nullptr;

    };
}


#endif //EDITOR_TEXTBUFFERAPI_H

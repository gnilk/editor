//
// Created by gnilk on 08.04.23.
//

#ifndef EDITOR_EDITORAPI_H
#define EDITOR_EDITORAPI_H

#include <functional>
#include <vector>
#include <string>
#include "Core/Editor.h"
#include "Core/Runloop.h"

#include "TextBufferAPI.h"

namespace gedit {
    class EditorAPI {
    public:
        using APIFunc = std::function<void(void)>;
    public:
        void ExitEditor() {
            Runloop::StopRunLoop();
        }
        TextBufferAPI::Ref GetActiveTextBuffer();
        std::vector<std::string> GetRegisteredLanguages();
        void NewBuffer(const char *name);
        TextBufferAPI::Ref LoadBuffer(const char *filename);

        std::vector<TextBufferAPI::Ref> GetBuffers();

        void SetActiveBuffer(TextBufferAPI::Ref activeBuffer);
    protected:
        APIFunc cbExitEditor = nullptr;

    };
}


#endif //EDITOR_EDITORAPI_H

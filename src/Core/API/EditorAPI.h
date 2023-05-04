//
// Created by gnilk on 08.04.23.
//

#ifndef EDITOR_EDITORAPI_H
#define EDITOR_EDITORAPI_H

#include <functional>
#include "Core/Editor.h"

#include "TextBufferAPI.h"

namespace gedit {
    class EditorAPI {
    public:
        using APIFunc = std::function<void(void)>;
    public:
        void ExitEditor() {
            cbExitEditor();
        }
        TextBufferAPI::Ref GetActiveTextBuffer();
    public:
        void SetExitEditorDelegate(APIFunc newExitEditor) {
            cbExitEditor = newExitEditor;
        }
    protected:
        APIFunc cbExitEditor = nullptr;

    };
}


#endif //EDITOR_EDITORAPI_H

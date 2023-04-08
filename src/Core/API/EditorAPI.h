//
// Created by gnilk on 08.04.23.
//

#ifndef EDITOR_EDITORAPI_H
#define EDITOR_EDITORAPI_H

#include <functional>

namespace gedit {
    class EditorAPI {
    public:
        using APIFunc = std::function<void(void)>;
    public:
        void ExitEditor() {
            cbExitEditor();
        }
    public:
        void SetExitEditorDelegate(APIFunc newExitEditor) {
            cbExitEditor = newExitEditor;
        }
    protected:
        APIFunc cbExitEditor = nullptr;

    };
}


#endif //EDITOR_EDITORAPI_H

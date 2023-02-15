//
// Created by gnilk on 14.02.23.
//

#ifndef EDITOR_EDITORVIEW_H
#define EDITOR_EDITORVIEW_H

#include "Core/ViewBase.h"
#include "Core/EditorMode.h"
#include "Core/Controllers/EditController.h"
#include "Core/Cursor.h"

#include "logger.h"

namespace gedit {
    class EditorView : public ViewBase {
    public:
        EditorView() = default;
        explicit EditorView(const Rect &viewArea) : ViewBase(viewArea) {

        }
        virtual ~EditorView() = default;

        void Begin() override;
        void OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) override;
        void DrawViewContents() override;

        EditController &GetEditController() {
            return editController;
        }

    private:
        void UpdateNavigation(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress);
        void DrawLines();

        void OnNavigateUp(int rows);
        void OnNavigateDown(int rows);

    private:
        ssize_t idxActiveLine = 0;
        int32_t viewTopLine = 0;
        int32_t viewBottomLine = 0;

        Cursor cursor = {};
        EditorMode editorMode;      // REMOVE
        EditController editController;

        gnilk::ILogger *logger = nullptr;
    };
}


#endif //EDITOR_EDITORVIEW_H

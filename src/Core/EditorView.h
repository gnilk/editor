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
            return viewData.editController;
        }

    private:
        bool UpdateNavigation(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress);

        void OnNavigateUp(int rows);
        void OnNavigateDown(int rows);

    private:
        // This is shared data...
        EditViewSharedData viewData;
        // --
        gnilk::ILogger *logger = nullptr;

    };
}


#endif //EDITOR_EDITORVIEW_H

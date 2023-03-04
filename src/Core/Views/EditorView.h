//
// Created by gnilk on 14.02.23.
//

#ifndef EDITOR_EDITORVIEW_H
#define EDITOR_EDITORVIEW_H

#include "Core/Controllers/EditController.h"
#include "Core/Cursor.h"

#include "ViewBase.h"

#include "logger.h"

namespace gedit {


    class EditorView : public ViewBase {
    public:
        EditorView() = default;
        explicit EditorView(const Rect &viewArea) : ViewBase(viewArea) {

        }
        virtual ~EditorView() = default;

        void InitView() override;

        EditController &GetEditController() {
            return viewData.editController;
        }
    protected:
        void OnKeyPress(const KeyPress &keyPress) override;
        void OnResized() override;
        void DrawViewContents() override;

    private:
        bool UpdateNavigation(const KeyPress &keyPress);

        void OnNavigateUpVSCode(int rows);
        void OnNavigateDownVSCode(int rows);

        void OnNavigateUpCLion(int rows);
        void OnNavigateDownCLion(int rows);

    private:
        // This is shared data...
        EditViewSharedData viewData;
        // --
        gnilk::ILogger *logger = nullptr;

    };
}


#endif //EDITOR_EDITORVIEW_H

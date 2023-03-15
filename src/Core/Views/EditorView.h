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
        void ReInitView() override;

        // Pass this on to someone who actually know how to deal with this - HStackView don't deal with height related stuff
        void MaximizeContentHeight() override {
            parentView->MaximizeContentHeight();
        }
        void RestoreContentHeight() override {
            parentView->RestoreContentHeight();
        }
        void ResetContentHeight() override {
            parentView->ResetContentHeight();
        }



        EditController &GetEditController() {
            return viewData.editController;
        }
    protected:
        void OnKeyPress(const KeyPress &keyPress) override;
        void OnResized() override;
        void OnActivate(bool isActive) override;
        void DrawViewContents() override;

    private:
        bool UpdateNavigation(const KeyPress &keyPress);

        void OnNavigateUpVSCode(int rows);
        void OnNavigateDownVSCode(int rows);

        void OnNavigateUpCLion(int rows);
        void OnNavigateDownCLion(int rows);

    private:
        bool bUseCLionPageNav = true;
        // This is shared data...
        EditViewSharedData viewData;
        // --
        gnilk::ILogger *logger = nullptr;

    };
}


#endif //EDITOR_EDITORVIEW_H

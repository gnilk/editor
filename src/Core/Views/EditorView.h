//
// Created by gnilk on 14.02.23.
//

#ifndef EDITOR_EDITORVIEW_H
#define EDITOR_EDITORVIEW_H

#include "Core/Controllers/EditController.h"
#include "Core/Cursor.h"
#include "Core/EditorModel.h"
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

        void SetEditorModel(EditorModel::Ref newEditorModel) {
            editorModel = newEditorModel;
        }

        void SetWindowCursor(const Cursor &cursor) override;

        bool OnAction(const KeyPressAction &kpAction) override;


    protected:
        void OnKeyPress(const KeyPress &keyPress) override;
        void OnResized() override;
        void OnActivate(bool isActive) override;
        void DrawViewContents() override;
    protected:
        // Action handlers
        bool OnActionLineDown(const KeyPressAction &kpAction);
        bool OnActionLineUp();
        bool OnActionPageUp();
        bool OnActionPageDown();
        bool OnActionStepLeft();
        bool OnActionStepRight();
        bool OnActionCommitLine();
        bool OnActionGotoFirstLine();   // First line of buffer
        bool OnActionGotoLastLine();    // Last line of buffer
        bool OnActionGotoTopLine();     // Top line of screen
        bool OnActionGotoBottomLine();    // Last visible line on screen
        bool OnActionWordRight();
        bool OnActionWordLeft();
        bool OnActionPreviousBuffer();
        bool OnActionNextBuffer();
        bool OnActionCycleActiveBuffer();
        bool OnActionLineHome();
        bool OnActionLineEnd();
        bool OnActionUndo();
//        bool OnActionBackspace();
        bool OnNextSearchResult();
        bool OnPrevSearchResult();

    private:
        bool DispatchAction(const KeyPressAction &kpAction);

        bool UpdateNavigation(const KeyPress &keyPress);

        void OnNavigateUpVSCode(int rows);
        void OnNavigateDownVSCode(int rows);

        void OnNavigateUpCLion(int rows);
        void OnNavigateDownCLion(int rows);

    private:
        bool bUseCLionPageNav = true;
        EditorModel::Ref editorModel;
        // --
        gnilk::ILogger *logger = nullptr;
    };
}


#endif //EDITOR_EDITORVIEW_H

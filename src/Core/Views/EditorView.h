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
#include "Core/VerticalNavigationViewModel.h"

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


        const std::u32string &GetStatusBarAbbreviation() override {
            static std::u32string defaultAbbr = U"EDT";
            return defaultAbbr;
        }
        std::pair<std::u32string, std::u32string> GetStatusBarInfo() override;

    protected:
        void OnKeyPress(const KeyPress &keyPress) override;
        void OnResized() override;
        void OnActivate(bool isActive) override;
        void DrawViewContents() override;
    protected:
        // Action handlers
        bool OnActionPreviousBuffer();
        bool OnActionNextBuffer();

    private:
        bool DispatchAction(const KeyPressAction &kpAction);
    private:
        bool bUseCLionPageNav = true;
        EditorModel::Ref editorModel;
        EditController::Ref editController;
        // --
        gnilk::ILogger *logger = nullptr;
    };
}


#endif //EDITOR_EDITORVIEW_H

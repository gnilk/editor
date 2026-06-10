//
// Created by gnilk on 14.02.23.
//

#ifndef EDITOR_EDITORVIEW_H
#define EDITOR_EDITORVIEW_H

#include "Core/Controllers/EditController.h"
#include "Core/Graphics/Cursor.h"
#include "Core/Document.h"
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

        void SetDocument(Document::Ref newDocument) {
            document = newDocument;
        }

        void SetWindowCursor(const Cursor &cursor) override;

        bool OnAction(const EditorAction &kpAction) override;


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
        bool DispatchAction(const EditorAction &kpAction);
        void DrawSearchResultOverlays(DrawContext &dc, int tabSize);
        void DrawSelectionOverlay(DrawContext &dc, int tabSize);
    private:
        bool bUseCLionPageNav = true;
        Document::Ref document;
        // The edit controller is a per-item member (1:1 with this view), like TerminalController in
        // TerminalView. It borrows the active document; we Attach/Detach it on document switch.
        EditController editController;
        // --
        gnilk::ILogger *logger = nullptr;
    };
}


#endif //EDITOR_EDITORVIEW_H

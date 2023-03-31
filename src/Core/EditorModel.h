//
// Created by gnilk on 17.03.23.
//

#ifndef EDITOR_EDITORMODEL_H
#define EDITOR_EDITORMODEL_H

#include "Controllers/EditController.h"
#include "Core/TextBuffer.h"
#include "Core/Cursor.h"

#include <memory>

namespace gedit {
    // This is the composite object linking TextBuffer/EditController together
    // It also holds data to reconstruct the view of the text (topLine/bottomLine)
    // Note: the cursor is owned by the view (for now) - but needs to move here, as it must follow..
    class EditorModel {
    public:
        using Ref = std::shared_ptr<EditorModel>;

        struct Selection {
            bool isActive = false;
            Cursor startPos = {};
            Cursor endPos = {};
        };

    public:
        EditorModel() = default;
        virtual ~EditorModel() = default;

        void Initialize(EditController::Ref newController, TextBuffer::Ref newTextBuffer) {
            editController = newController;
            textBuffer = newTextBuffer;

            editController->SetTextBuffer(textBuffer);

        }

        EditController::Ref GetEditController() {
            return editController;
        }
        TextBuffer::Ref GetTextBuffer() {
            return textBuffer;
        }

        // proxy
        std::vector<Line *> &Lines() {
            return editController->Lines();
        }
        Line *LineAt(size_t idxLine) {
            return editController->LineAt(idxLine);
        }

        bool IsActive() {
            return isActive;
        }
        void SetActive(bool newIsActive) {
            isActive = newIsActive;
        }
        void BeginSelection() {
            currentSelection.isActive = true;
            currentSelection.startPos = cursor;
            currentSelection.endPos = cursor;
        }
        void UpdateSelection() {
            // perhaps check if active...
            currentSelection.endPos = cursor;
        }
        void CancelSelection() {
            currentSelection.isActive = false;
        }
        bool IsSelectionActive() {
            return currentSelection.isActive;
        }
        const Selection &GetSelection() {
            return currentSelection;
        }

    public:
        Cursor cursor;
        int32_t idxActiveLine = 0;
        int32_t viewTopLine = 0;
        int32_t viewBottomLine = 0;
    private:
        EditController::Ref editController = nullptr;     // Pointer???
        Selection currentSelection = {};
        TextBuffer::Ref textBuffer = nullptr;             // This is 'owned' by BufferManager
        bool isActive = false;

    };
}

#endif //EDITOR_EDITORMODEL_H

//
// Created by gnilk on 17.03.23.
//

#ifndef EDITOR_EDITORMODEL_H
#define EDITOR_EDITORMODEL_H

#include "Controllers/EditController.h"
#include "Core/TextBuffer.h"
#include "Core/Cursor.h"
#include "Core/KeyPress.h"

#include <memory>

namespace gedit {

    class EditorModel;
    struct SearchResult {
        size_t idxLine;
        size_t cursor_x;
        size_t length;
    };
    // NOTE: Selection coordinates are in TextBuffer coordinates!!!!!
    struct Selection {
        friend EditorModel;
        bool IsSelected(int x, int y) {
            if (!isActive) return false;
            if (y < startPos.y) return false;
            if (y > endPos.y) return false;

            if ((y == startPos.y) && (x < startPos.x)) return false;
            if ((y == endPos.y) && (x > endPos.x)) return false;

            return true;
        }

        bool IsLineSelected(int y) {
            return IsSelected(0, y);
        }

        bool IsActive() const {
            return isActive;
        }
        //
        // This returns the coords sorted!!!
        //
        const Point &GetStart() {
            if (endPos > startPos) {
                return startPos;
            }
            return endPos;
        }

        const Point &GetStart() const {
            if (endPos > startPos) {
                return startPos;
            }
            return endPos;
        }

        const Point &GetEnd() {
            if (endPos < startPos) {
                return startPos;
            }
            return endPos;
        }

        const Point &GetEnd() const {
            if (endPos < startPos) {
                return startPos;
            }
            return endPos;
        }


    protected:
        // Consider making these private...
        bool isActive = false;
        Point startPos = {};
        Point endPos = {};

    };


    // This is the composite object linking TextBuffer/EditController together
    // It also holds data to reconstruct the view of the text (topLine/bottomLine)
    // Note: the cursor is owned by the view (for now) - but needs to move here, as it must follow..
    class EditorModel {
    public:
        using Ref = std::shared_ptr<EditorModel>;

    public:
        EditorModel() = default;
        virtual ~EditorModel() {
            // note: this is just here for debugging purposes..
            // printf("EditorModel::DTOR\n");
        }

        void Initialize(EditController::Ref newController, TextBuffer::Ref newTextBuffer) {
            editController = newController;
            editController->Begin();

            textBuffer = newTextBuffer;
            editController->SetTextBuffer(textBuffer);
        }

        static Ref Create();

        void Close() {
            textBuffer->Close();
        }

        EditController::Ref GetEditController() {
            return editController;
        }
        TextBuffer::Ref GetTextBuffer() {
            return textBuffer;
        }

        Cursor &GetCursor() {
            return lineCursor.cursor;
        }

        // proxy
        const std::vector<Line::Ref> &Lines() {
            return editController->Lines();
        }
        Line::Ref LineAt(size_t idxLine) {
            return editController->LineAt(idxLine);
        }
        Line::Ref ActiveLine() {
            return editController->LineAt(lineCursor.idxActiveLine);
        }
        size_t GetActiveLineIndex() {
            return lineCursor.idxActiveLine;
        }

        bool IsActive() {
            return isActive;
        }
        void SetActive(bool newIsActive) {
            isActive = newIsActive;
        }
        void BeginSelection() {
            currentSelection.isActive = true;
            currentSelection.startPos.x = lineCursor.cursor.position.x;
            currentSelection.startPos.y = lineCursor.idxActiveLine;

            currentSelection.endPos = currentSelection.startPos;
        }
        void UpdateSelection() {
            // perhaps check if active...
            Point newEnd(lineCursor.cursor.position.x, lineCursor.idxActiveLine);
            currentSelection.endPos = newEnd;

        }
        void RefocusViewArea();
        void CancelSelection() {
            currentSelection.isActive = false;
        }
        bool IsSelectionActive() {
            return currentSelection.isActive;
        }
        const Selection &GetSelection() {
            return currentSelection;
        }

        const Selection &GetSelection() const {
            return currentSelection;
        }
        void DeleteSelection();
        void CommentSelectionOrLine();

        size_t SearchFor(const std::u32string &searchItem);
        void ClearSearchResults();
        bool HaveSearchResults() {
            return !searchResults.empty();
        }
        size_t GetSearchHitIndex();
        bool JumpToSearchHit(size_t idxHit);
        void NextSearchResult();
        void PrevSearchResult();
        void ResetSearchHitIndex();

        bool LoadData(const std::filesystem::path &pathName);
        bool SaveData(const std::filesystem::path &pathName);
        bool SaveDataNoChangeCheck(const std::filesystem::path &pathName);



    public:
        LineCursor &GetLineCursor() {
            return lineCursor;
        }
        LineCursor::Ref  GetLineCursorRef() {
            return &lineCursor;
        }
        // Move the following to a separate structure
        // REMOVE ANY DUPLICATION - search for 'idxActiveLine' (EditorView and such)
//        Cursor cursor;
//        size_t idxActiveLine = 0;
//        int32_t viewTopLine = 0;
//        int32_t viewBottomLine = 0;
        //

        std::vector<SearchResult> searchResults;
        size_t idxActiveSearchHit = 0;
    private:

        LineCursor lineCursor;

        EditController::Ref editController = nullptr;     // Pointer???
        Selection currentSelection = {};
        TextBuffer::Ref textBuffer = nullptr;             // This is 'owned' by BufferManager
        bool isActive = false;

    };
}

#endif //EDITOR_EDITORMODEL_H

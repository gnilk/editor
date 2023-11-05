//
// Created by gnilk on 17.03.23.
//

#ifndef EDITOR_EDITORMODEL_H
#define EDITOR_EDITORMODEL_H

#include "Core/TextBuffer.h"
#include "Core/Cursor.h"
#include "Core/KeyPress.h"
#include "Core/VerticalNavigationViewModel.h"
#include "Core/Rect.h"
#include "Core/UndoHistory.h"
#include "Core/KeyMapping.h"

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
        EditorModel(TextBuffer::Ref newTextBuffer) : textBuffer(newTextBuffer) {
        }
        virtual ~EditorModel() {
            // note: this is just here for debugging purposes..
            // printf("EditorModel::DTOR\n");
        }
        static Ref Create(TextBuffer::Ref newTextBuffer);
        void Begin();
        void OnViewInit(const Rect &rect);

        void Close() {
            textBuffer->Close();
        }
        void SetViewRect(const Rect &rect) {
            viewRect = rect;
        }

        TextBuffer::Ref GetTextBuffer() {
            return textBuffer;
        }

        Cursor &GetCursor() {
            return lineCursor.cursor;
        }

        // proxy
        const std::vector<Line::Ref> &Lines() {
            return textBuffer->Lines();
        }
        Line::Ref LineAt(size_t idxLine) {
            return textBuffer->LineAt(idxLine);
        }
        Line::Ref ActiveLine() {
            return textBuffer->LineAt(lineCursor.idxActiveLine);
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
        void IndentSelectionOrLine();
        void UnindentSelectionOrLine();
        void CommentSelectionOrLine();

        void AddLineComment(size_t idxLineStart, size_t idxLineEnd, const std::u32string &lineCommentPrefix);
        void IndentLines(size_t idxLineStart, size_t idxLineEnd);
        void UnindentLines(size_t idxLineStart, size_t idxLineEnd);

        void AddTab(Cursor &cursor, size_t idxActiveLine);
        void DelTab(Cursor &cursor, size_t idxActiveLine);

        void AddCharToLineNoUndo(Cursor &cursor, Line::Ref line, char32_t ch);
        void RemoveCharFromLineNoUndo(gedit::Cursor &cursor, Line::Ref line);

        void UpdateModelFromNavigation(bool updateCursor);


            // FIXME: Cursor and idxActiveLine not needed
        void Undo(Cursor &cursor, size_t &idxActiveLine);

        UndoHistory::UndoItem::Ref BeginUndoItem();
        UndoHistory::UndoItem::Ref BeginUndoFromLineRange(size_t idxStart, size_t idxEnd);
        void EndUndoItem(UndoHistory::UndoItem::Ref undoItem);

        // protected..
        void UpdateSyntaxForBuffer();
        Job::Ref UpdateSyntaxForActiveLineRegion();
        Job::Ref UpdateSyntaxForRegion(size_t idxStartLine, size_t idxEndLine);


        size_t NewLine(size_t idxActiveLine, Cursor &cursor);

        void DeleteLinesNoSyntaxUpdate(size_t idxLineStart, size_t idxLineEnd);
        void DeleteRange(const Point &startPos, const Point &endPos);


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


        void PasteFromClipboard();

        bool OnAction(const KeyPressAction &kpAction);
        bool DispatchAction(const KeyPressAction &kpAction);
    protected:
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
        bool OnActionLineHome();
        bool OnActionLineEnd();
        bool OnActionUndo();
        bool OnNextSearchResult();
        bool OnPrevSearchResult();

        bool bUseCLionPageNav = true;

    public:
        LineCursor &GetLineCursor() {
            return lineCursor;
        }
        LineCursor::Ref  GetLineCursorRef() {
            return &lineCursor;
        }

        std::vector<SearchResult> searchResults;
        size_t idxActiveSearchHit = 0;
    private:
        gnilk::Log::Ref logger;
        LineCursor lineCursor;
        Selection currentSelection = {};
        VerticalNavigationViewModel verticalNavigationViewModel;
        Rect viewRect = {};
        UndoHistory historyBuffer;

        TextBuffer::Ref textBuffer = nullptr;             // This is 'owned' by BufferManager
        bool isActive = false;

    };
}

#endif //EDITOR_EDITORMODEL_H

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
        size_t GetStartLine() const {
            // Did we select backwards???
            if (endPos.y > startPos.y) {
                return startPos.y;
            }
            return endPos.y;
        }
        size_t GetStartLine() {
            // Did we select backwards???
            if (endPos.y > startPos.y) {
                return startPos.y;
            }
            return endPos.y;
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
        // Setters
        void SetStart(const Point &pt) {
            startPos = pt;
        }
        void SetStartYPos(const size_t yp) {
            startPos.y = yp;
        }
        void SetEndYPos(const size_t yp) {
            endPos.y = yp;
        }
        void SetEnd(const Point &pt) {
            endPos = pt;
        }
        void SetStartLine(const size_t startLine) {
            idxStartLine = startLine;
        }
        void SetActive(const bool newActive) {
            isActive = newActive;
        }



    private:

        bool isActive = false;
        size_t idxStartLine;
        Point startPos = {};    // buffer coords
        Point endPos = {};      // buffer coords

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

        // FIXME: Rename this
        void OnViewInit(const Rect &rect);
        void RefocusViewArea();

        void Close() {
            textBuffer->Close();
        }

        TextBuffer::Ref GetTextBuffer() {
            return textBuffer;
        }

        bool IsActive() {
            return isActive;
        }
        void SetActive(bool newIsActive) {
            isActive = newIsActive;
        }


        // proxies
        __inline const std::vector<Line::Ref> &Lines() {
            return textBuffer->Lines();
        }
        __inline Line::Ref LineAt(size_t idxLine) {
            return textBuffer->LineAt(idxLine);
        }
        __inline Line::Ref ActiveLine() {
            return textBuffer->LineAt(lineCursor.idxActiveLine);
        }

        void AddLineComment(size_t idxLineStart, size_t idxLineEnd, const std::u32string &lineCommentPrefix);
        void IndentLines(size_t idxLineStart, size_t idxLineEnd);
        void UnindentLines(size_t idxLineStart, size_t idxLineEnd);

        // Not quite sure - they are conflicting with Indent/Unindent
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

        // should be protected?
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

        Cursor &GetCursor() {
            return lineCursor.cursor;
        }

        LineCursor &GetLineCursor() {
            return lineCursor;
        }
        LineCursor::Ref  GetLineCursorRef() {
            return &lineCursor;
        }

        void PasteFromClipboard();

        bool OnAction(const KeyPressAction &kpAction);
        bool DispatchAction(const KeyPressAction &kpAction);

        // Selection functions - not sure these must be exposed - perhaps for API purposes?
        void BeginSelection() {
            currentSelection.SetActive(true);
            currentSelection.SetStartLine(lineCursor.idxActiveLine);
            currentSelection.SetStart(lineCursor.cursor.position);
            currentSelection.SetEnd(lineCursor.cursor.position);

            currentSelection.SetStartYPos(lineCursor.idxActiveLine);
            currentSelection.SetEndYPos(lineCursor.idxActiveLine);
        }
        __inline bool IsSelectionActive() {
            return currentSelection.IsActive();
        }
        __inline const Selection &GetSelection() {
            return currentSelection;
        }
        __inline void CancelSelection() {
            currentSelection.SetActive(false);
        }
        __inline void RestoreCursorFromSelection() {
            lineCursor.idxActiveLine = currentSelection.GetStartLine();
            lineCursor.cursor.position = currentSelection.GetStart();

            verticalNavigationViewModel->OnNavigateDown(0, viewRect, Lines().size());
        }
        void DeleteSelection();     // Fixme: naming - this looks like a selection-range mgmt function

    protected:
        void UpdateSelection() {
            // perhaps check if active...
            Point newEnd(lineCursor.cursor.position.x, lineCursor.idxActiveLine);
            currentSelection.SetEnd(newEnd);

        }

        void IndentSelectionOrLine();
        void UnindentSelectionOrLine();
        void CommentSelectionOrLine();


    protected:
        void Begin();


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

        std::vector<SearchResult> searchResults;
        size_t idxActiveSearchHit = 0;
    private:
        gnilk::Log::Ref logger;
        LineCursor lineCursor;
        Selection currentSelection = {};
        VerticalNavigationViewModel::Ref verticalNavigationViewModel = nullptr;
        Rect viewRect = {};
        UndoHistory historyBuffer;

        TextBuffer::Ref textBuffer = nullptr;             // This is 'owned' by BufferManager
        bool isActive = false;

    };
}

#endif //EDITOR_EDITORMODEL_H

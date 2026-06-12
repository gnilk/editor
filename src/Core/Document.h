//
// Created by gnilk on 17.03.23.
//

#ifndef EDITOR_DOCUMENT_H
#define EDITOR_DOCUMENT_H

#include "Core/TextBuffer.h"
#include "Core/Graphics/Cursor.h"
#include "Core/KeyPress.h"
#include "Core/VerticalNavigationViewModel.h"
#include "Core/Rect.h"
#include "Core/UndoHistory.h"
#include "Core/KeyMapping.h"
#include "Core/DocumentViewState.h"
#include "Core/EditState.h"

#include <memory>

namespace gedit {

    class Document;
    struct SearchResult {
        size_t idxLine;
        size_t cursor_x;
        size_t length;
    };
    // NOTE: 'Selection' moved to DocumentViewState.h (Phase 2 - it is per-view state, not document data).

    // The open document: owns its TextBuffer and file identity (path), plus the editing state that
    // sits between the text data and the view (cursor, selection, undo, search). The EditController
    // references this document (it does not live here). Workspace owns the lifetime of open documents.
    class Document {
    public:
        using Ref = std::shared_ptr<Document>;

    public:
        Document() = default;
        Document(TextBuffer::Ref newTextBuffer) : textBuffer(newTextBuffer) {
        }
        virtual ~Document() {
            // note: this is just here for debugging purposes..
            // printf("Document::DTOR\n");
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

        // proxies
        __inline const std::vector<Line::Ref> &Lines() {
            return textBuffer->Lines();
        }
        __inline Line::Ref LineAt(size_t idxLine) {
            return textBuffer->LineAt(idxLine);
        }
        __inline Line::Ref ActiveLine() {
            return textBuffer->LineAt(documentViewState->lineCursor.idxActiveLine);
        }

        void AddLineComment(size_t idxLineStart, size_t idxLineEnd, const std::u32string &lineCommentPrefix);
        void IndentLines(size_t idxLineStart, size_t idxLineEnd);
        void UnindentLines(size_t idxLineStart, size_t idxLineEnd);
        // Reformat: re-derive the indentation of lines [startY..endY] (inclusive) via the indent engine,
        // seeded from the trusted line above and forward-extended through any construct it ends inside.
        void ReindentLineRange(size_t startY, size_t endY);

        void AddTab();
        void DelTab();

        void AddCharToLineNoUndo(Cursor &cursor, Line::Ref line, char32_t ch);
        void RemoveCharFromLineNoUndo(gedit::Cursor &cursor, Line::Ref line);

        void UpdateDocumentFromNavigation(bool updateCursor);

        // Visual tab width for the active language (falls back to 4 if no language is set)
        int GetTabSize();
        // wantedColumn is a *visual* column (tab-expanded), so vertical navigation preserves the
        // on-screen column across lines with differing tab/space layouts. Capture stores the visual
        // column of the cursor; Apply maps a stored visual column back to a character index.
        void CaptureWantedColumn(Cursor &cursor, const Line::Ref &line);
        void CaptureWantedColumn() { CaptureWantedColumn(documentViewState->lineCursor.cursor, ActiveLine()); }
        void ApplyWantedColumn(Cursor &cursor, const Line::Ref &line);


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

        // File identity. A document knows the path it loads from / saves to; this is the document's own
        // identity, not something the browse-tree node lends it. The no-arg Load/Save use it.
        const std::filesystem::path &GetPath() const {
            return path;
        }
        void SetPath(const std::filesystem::path &newPath) {
            path = newPath;
        }
        bool Load();
        bool Save();
        bool SaveForce();

        Cursor &GetCursor() {
            return documentViewState->lineCursor.cursor;
        }

        LineCursor &GetLineCursor() {
            return documentViewState->lineCursor;
        }
        LineCursor::Ref  GetLineCursorRef() {
            return &documentViewState->lineCursor;
        }

        DocumentViewMode GetViewMode() const {
            return documentViewState->viewMode;
        }
        void SetViewMode(DocumentViewMode newViewMode) {
            documentViewState->viewMode = newViewMode;
        }

        // Move the caret to an absolute (line, charIdx) text position and refocus the text view around
        // it. The single canonical way to set the cursor from outside navigation - used by search jumps
        // and by the HexView write-back (a hex-space move translated back into text coords).
        void SetCursorPosition(size_t idxLine, size_t idxChar);

        void PasteFromClipboard();

        bool OnAction(const EditorAction &kpAction);
        bool DispatchAction(const EditorAction &kpAction);

        // Selection functions - not sure these must be exposed - perhaps for API purposes?
        void BeginSelection() {
            documentViewState->currentSelection.SetActive(true);
            documentViewState->currentSelection.SetStartLine(documentViewState->lineCursor.idxActiveLine);
            documentViewState->currentSelection.SetStart(documentViewState->lineCursor.cursor.position);
            documentViewState->currentSelection.SetEnd(documentViewState->lineCursor.cursor.position);

            documentViewState->currentSelection.SetStartYPos(documentViewState->lineCursor.idxActiveLine);
            documentViewState->currentSelection.SetEndYPos(documentViewState->lineCursor.idxActiveLine);
        }
        __inline bool IsSelectionActive() {
            return documentViewState->currentSelection.IsActive();
        }
        __inline const Selection &GetSelection() {
            return documentViewState->currentSelection;
        }
        __inline void CancelSelection() {
            documentViewState->currentSelection.SetActive(false);
        }
        __inline void RestoreCursorFromSelection() {
            documentViewState->lineCursor.idxActiveLine = documentViewState->currentSelection.GetStartLine();
            documentViewState->lineCursor.cursor.position = documentViewState->currentSelection.GetStart();

            verticalNavigationViewModel->OnNavigateDown(0, viewRect, Lines().size());
        }
        void DeleteSelection();     // Fixme: naming - this looks like a selection-range mgmt function

    protected:
        void UpdateSelection() {
            // perhaps check if active...
            Point newEnd(documentViewState->lineCursor.cursor.position.x, documentViewState->lineCursor.idxActiveLine);
            documentViewState->currentSelection.SetEnd(newEnd);

        }

        void IndentSelectionOrLine();
        void UnindentSelectionOrLine();
        void CommentSelectionOrLine();


    protected:
        void Begin();


        bool OnActionLineDown(const EditorAction &kpAction);
        bool OnActionLineUp();
        bool OnActionPageUp();
        bool OnActionPageDown();
        bool OnActionStepLeft();
        bool OnActionStepRight();
        bool OnActionCommitLine();
        bool OnActionIndent();
        bool OnActionUnindent();
        bool OnActionReformatLine();
        bool OnActionReformatBlock();
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
        // Per-view state (cursor + selection), referenced not fused. See DocumentViewState.h. Today a
        // document references exactly one; a split/side-by-side view will later give each view-item
        // its own and leave the document holding a saved snapshot.
        DocumentViewState::Ref documentViewState = DocumentViewState::Create();
        VerticalNavigationViewModel::Ref verticalNavigationViewModel = nullptr;
        Rect viewRect = {};
        // Per-document edit history (undo), referenced not fused. See EditState.h. All views of one
        // buffer will share this single EditState while keeping independent DocumentViewState cursors.
        EditState::Ref editState = EditState::Create();

        TextBuffer::Ref textBuffer = nullptr;             // The document owns its buffer
        std::filesystem::path path;                       // File identity (load/save target)

    };
}

#endif //EDITOR_DOCUMENT_H

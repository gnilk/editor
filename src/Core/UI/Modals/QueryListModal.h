//
// An edit line with a result list below it (docs/modal_fixes.md §3.4 item 2). Each edit runs the query
// again; Up/Down/PageUp/PageDown walk the results (each move is previewed), Return commits the selected
// one, Esc cancels. Left/Right/Home/End/Backspace/Delete edit the line.
//
// Generic: a subclass supplies the rows for a query and decides what preview / commit / cancel mean
// (Find over the active document; later 'search everywhere').
//

#ifndef GOATEDIT_QUERYLISTMODAL_H
#define GOATEDIT_QUERYLISTMODAL_H

#include <string>
#include <vector>

#include "ModalView.h"
#include "Core/Line.h"
#include "Core/UI/Controllers/BaseController.h"

namespace gedit {
    class QueryListModal : public ModalView {
    public:
        QueryListModal(const std::string &caption, const std::u32string &prompt);
        virtual ~QueryListModal() = default;

        // Set the text (e.g. from a selection) and run the query
        void SetQuery(const std::u32string &query);
        const std::u32string &GetQuery() const {
            return input->Buffer();
        }

        // -1 when there are no results
        int GetSelectedRow() const {
            return idxSelected;
        }
        size_t GetNumRows() const {
            return rows.size();
        }

        bool OnAction(const EditorAction &kpAction) override;
        void OnKeyPress(const KeyPress &keyPress) override;
    protected:
        // The rows to list for this query (one line each)
        virtual std::vector<std::u32string> OnQuery(const std::u32string &query) = 0;
        // The selection moved (-1: no results)
        virtual void OnPreview(int idxRow) {}
        // Return on a row
        virtual void OnCommit(int idxRow) {}
        // Esc
        virtual void OnCancel() {}

        // Insert at the edit cursor (e.g. a paste) and run the query again - only up to the first line break
        void InsertText(const std::u32string &text);

        void OnClosed(bool wasCancelled) override;
        void DrawViewContents() override;
        Rect ComputeRect(const Rect &screenRect) override;
    private:
        void RunQuery();
        void Select(int idxRow);
        int ListHeight();
    private:
        std::u32string prompt;
        Line::Ref input = nullptr;
        Cursor inputCursor = {};
        BaseController inputController;

        std::vector<std::u32string> rows;
        int idxSelected = -1;
        int idxTopRow = 0;
        int idxCommitted = -1;
    };
}

#endif //GOATEDIT_QUERYLISTMODAL_H

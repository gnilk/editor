//
// Find in the active document (docs/modal_fixes.md §3.4 item 2, step 4). Case-insensitive plain text, one
// row per line with a hit ('  12: text'). Moving in the list previews the hit in the editor; Return
// leaves the cursor there and keeps the results for Next/Prev; Esc puts the cursor and the view back.
//

#ifndef GOATEDIT_FINDMODAL_H
#define GOATEDIT_FINDMODAL_H

#include <memory>

#include "Core/Document.h"
#include "Core/UI/Modals/QueryListModal.h"

namespace gedit {
    class FindModal : public QueryListModal {
    public:
        using Ptr = std::unique_ptr<FindModal>;
    public:
        explicit FindModal(Document::Ref document);
        virtual ~FindModal() = default;

        static Ptr Create(Document::Ref document) {
            return std::make_unique<FindModal>(document);
        }

        // The single-line selection, if any - what Find starts with
        static std::u32string SelectedText(Document::Ref document);
    protected:
        std::vector<std::u32string> OnQuery(const std::u32string &query) override;
        void OnPreview(int idxRow) override;
        void OnCommit(int idxRow) override;
        void OnCancel() override;
    private:
        void Restore();
    private:
        Document::Ref document = nullptr;
        // Where we were when Find opened
        size_t idxStartLine = 0;
        size_t idxStartChar = 0;
        int32_t startViewTopLine = 0;
    };
}

#endif //GOATEDIT_FINDMODAL_H

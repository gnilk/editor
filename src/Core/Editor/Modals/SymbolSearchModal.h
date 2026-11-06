//
// Go to symbol (docs/code-index.md §4.6): a name search over the whole code index. Each row is a kind marker
// and the qualified name, with 'path:line' on the right. Moving in the list previews a hit in the active
// document; Return opens the file at the symbol; Esc puts the cursor and the view back.
//

#ifndef GOATEDIT_SYMBOLSEARCHMODAL_H
#define GOATEDIT_SYMBOLSEARCHMODAL_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Core/Document.h"
#include "Core/Index/CodeIndex.h"
#include "Core/UI/Modals/QueryListModal.h"

namespace gedit {
    class SymbolSearchModal : public QueryListModal {
    public:
        using Ptr = std::unique_ptr<SymbolSearchModal>;
    public:
        // 'activeDocument' may be nullptr. 'roots' shorten the paths shown - nothing is searched by them.
        SymbolSearchModal(CodeIndex::Ref index, Document::Ref activeDocument, std::string activePath, std::vector<std::filesystem::path> roots);
        virtual ~SymbolSearchModal() = default;

        static Ptr Create(CodeIndex::Ref index, Document::Ref activeDocument, std::string activePath, std::vector<std::filesystem::path> roots) {
            return std::make_unique<SymbolSearchModal>(std::move(index), std::move(activeDocument), std::move(activePath), std::move(roots));
        }

        // A fixed list to choose from - the candidates of a go-to, best first. Typing filters it.
        static Ptr CreateForHits(std::vector<CodeIndex::Hit> hits, Document::Ref activeDocument, std::string activePath, std::vector<std::filesystem::path> roots);

        // One letter for the kind of symbol; for functions and methods upper case is the body, lower the declaration
        static char32_t KindMarker(const CodeIndex::Hit &hit);
        // 'src/Core/Document.cpp:12' - relative to the root that holds the file, 1-based line
        static std::string HitLocation(const CodeIndex::Hit &hit, const std::vector<std::filesystem::path> &roots);

        const std::vector<CodeIndex::Hit> &GetHits() const {
            return hits;
        }
    protected:
        std::vector<std::u32string> OnQuery(const std::u32string &query) override;
        std::u32string OnRowDetail(int idxRow) override;
        void OnPreview(int idxRow) override;
        void OnCommit(int idxRow) override;
        void OnCancel() override;
    private:
        void RestoreActiveDocument();
    private:
        CodeIndex::Ref index = nullptr;     // nullptr: 'fixedHits' is all there is
        std::vector<CodeIndex::Hit> fixedHits;
        Document::Ref activeDocument = nullptr;
        std::string activePath;
        std::vector<std::filesystem::path> roots;
        std::vector<CodeIndex::Hit> hits;
        // Where we were when the modal opened
        size_t idxStartLine = 0;
        size_t idxStartChar = 0;
        int32_t startViewTopLine = 0;
    };
}

#endif //GOATEDIT_SYMBOLSEARCHMODAL_H

//
// How a type fits in (docs/code-index.md §5.5): its bases, derived types, what it holds and who holds it, as a tree that
// opens one level at a time. Return jumps to the row's place, through the navigation stack.
//

#ifndef GOATEDIT_RELATIONSHIPMODAL_H
#define GOATEDIT_RELATIONSHIPMODAL_H

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Core/Index/CodeIndex.h"
#include "Core/Index/RelationshipTree.h"
#include "Core/Line.h"
#include "Core/UI/Modals/TreeSelectionModal.h"

namespace gedit {
    class RelationshipModal : public TreeSelectionModal<RelationshipTree::Row> {
    public:
        using Ptr = std::unique_ptr<RelationshipModal>;
        using Tree = TreeView<RelationshipTree::Row>;
        // A line of code with its colours - nullptr if there is none
        using LineSource = std::function<Line::Ref(const std::string &path, uint32_t line)>;
    public:
        // 'roots' shorten the paths shown, 'lineSource' gives the code a relation is written in
        RelationshipModal(CodeIndex::Ref index, const std::vector<std::u32string> &types, std::vector<std::filesystem::path> roots, LineSource lineSource);
        virtual ~RelationshipModal() = default;

        static Ptr Create(CodeIndex::Ref index, const std::vector<std::u32string> &types, std::vector<std::filesystem::path> roots, LineSource lineSource) {
            return std::make_unique<RelationshipModal>(std::move(index), types, std::move(roots), std::move(lineSource));
        }

        bool OnAction(const EditorAction &kpAction) override;
        // A code row in its colours, or a relation with its type name in the type colour, within 'width' columns. False for a
        // row that is plain: a title, a name that says nothing about the architecture, or one that doesn't fit.
        bool DrawRow(DrawContext &dc, int x, int y, int width, const RelationshipTree::Row &row);
        const std::optional<RelationshipTree::Row> &GetCommitted() const {
            return committed;
        }
    protected:
        void OnClosed(bool wasCancelled) override;
        Rect ComputeRect(const Rect &screenRect) override;
    private:
        void FetchChildren(Tree::TreeNode::Ref node);
        bool DrawSourceRow(DrawContext &dc, int x, int y, int width, const RelationshipTree::Row &row);
        bool DrawRelationRow(DrawContext &dc, int x, int y, int width, const RelationshipTree::Row &row);
        void AddRow(Tree::TreeNode::Ref parent, const RelationshipTree::Row &row);
        void AddSourceRow(Tree::TreeNode::Ref parent, const RelationshipTree::Place &place);
        std::string RowText(const RelationshipTree::Row &row) const;
    private:
        CodeIndex::Ref index = nullptr;
        std::vector<std::filesystem::path> roots;
        LineSource lineSource = nullptr;
        // The code of the rows opened so far, trimmed. A copy: the rows don't follow an edit, and the modal blocks those.
        std::map<std::pair<std::string, uint32_t>, Line::Ref> sourceLines;
        std::optional<RelationshipTree::Row> committed;
    };
}

#endif //GOATEDIT_RELATIONSHIPMODAL_H

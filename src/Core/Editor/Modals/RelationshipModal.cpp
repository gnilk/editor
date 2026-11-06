//
// The relationship tree - see RelationshipModal.h
//

#include <set>

#include "RelationshipModal.h"
#include "SymbolSearchModal.h"
#include "Core/ActionHelper.h"
#include "Core/Editor.h"
#include "Core/Editor/LineRender.h"
#include "Core/UI/UIHost.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

RelationshipModal::RelationshipModal(CodeIndex::Ref index, const std::vector<std::u32string> &types, std::vector<std::filesystem::path> roots, LineSource lineSource) :
    index(std::move(index)), roots(std::move(roots)), lineSource(std::move(lineSource)) {
    SetCaption("Relationships");
    auto tree = GetTree();
    tree->SetToStringDelegate([this](const RelationshipTree::Row &row) {
        return RowText(row);
    });
    tree->SetDimDelegate([](const RelationshipTree::Row &row) {
        return row.isOutside || row.isMacro;
    });
    tree->SetDrawRowDelegate([this](DrawContext &dc, int x, int y, int width, const RelationshipTree::Row &row) {
        return DrawRow(dc, x, y, width, row);
    });
    tree->cbFetchChildrenForNode = [this](Tree::TreeNode::Ref node) {
        FetchChildren(std::move(node));
    };
    for (auto &type : types) {
        AddRow(tree->GetRootNode(), RelationshipTree::TypeRow(*this->index, type));
    }
    if (!types.empty()) {
        tree->Expand();
    }
}

void RelationshipModal::AddRow(Tree::TreeNode::Ref parent, const RelationshipTree::Row &row) {
    auto tree = GetTree();
    auto node = tree->AddItem(parent, row);
    node->hasUnfetchedChildren = !row.types.empty() || !row.places.empty();
}

void RelationshipModal::AddSourceRow(Tree::TreeNode::Ref parent, const RelationshipTree::Place &place) {
    auto line = lineSource(place.path, place.line);
    if (line == nullptr) {
        AddRow(parent, RelationshipTree::SourceRow(place, U""));
        return;
    }
    AddRow(parent, RelationshipTree::SourceRow(place, line->Buffer()));
    auto idxText = line->Buffer().find_first_not_of(U" \t");
    auto trimmed = line->CopyOfRange((idxText == std::u32string::npos) ? line->Length() : idxText, line->Length());
    // The modal's background, not the editor's
    auto &uiColors = UIHost::Instance().GetUIColors();
    if (uiColors.HasColor("background")) {
        for (auto &attrib : trimmed->Attributes()) {
            attrib.backgroundColor = uiColors["background"];
        }
    }
    sourceLines[{place.path, place.line}] = trimmed;
}

// Called by the tree when a row opens - the index is asked then, and only then
void RelationshipModal::FetchChildren(Tree::TreeNode::Ref node) {
    auto tree = GetTree();
    auto &row = node->data;
    for (auto &place : row.places) {
        AddSourceRow(node, place);
    }
    if (row.types.size() > 1) {
        for (auto &type : row.types) {
            AddRow(node, RelationshipTree::TypeRow(*index, type));
        }
        return;
    }
    if (row.types.empty()) {
        return;
    }
    std::set<std::u32string> ancestors;
    for (auto ancestor = node; ancestor != nullptr; ancestor = ancestor->parent) {
        if (ancestor->data.types.size() == 1) {
            ancestors.insert(ancestor->data.types[0]);
        }
    }
    for (auto &group : RelationshipTree::RelationsOf(*index, row.types[0], ancestors)) {
        auto groupNode = tree->AddItem(node, group.title);
        groupNode->isExpanded = true;
        for (auto &groupRow : group.rows) {
            AddRow(groupNode, groupRow);
        }
    }
}

bool RelationshipModal::DrawRow(DrawContext &dc, int x, int y, int width, const RelationshipTree::Row &row) {
    return row.isSource ? DrawSourceRow(dc, x, y, width, row) : DrawRelationRow(dc, x, y, width, row);
}

bool RelationshipModal::DrawRelationRow(DrawContext &dc, int x, int y, int width, const RelationshipTree::Row &row) {
    if (row.path.empty() || row.isOutside || row.isMacro) {
        return false;
    }
    auto text = UnicodeHelper::utf8to32(RowText(row));
    auto idxName = text.find(row.text);
    if ((idxName == std::u32string::npos) || ((x + (int)text.size()) >= width)) {
        return false;
    }
    auto &uiColors = UIHost::Instance().GetUIColors();
    auto foreground = uiColors["foreground"];
    auto background = uiColors["background"];
    auto idxRest = idxName + row.text.size();
    dc.DrawStringAt(x, y, text.substr(0, idxName));
    dc.SetColor(Editor::Instance().ColorFromLanguageToken(kLanguageTokenClass::kUserType).first, background);
    dc.DrawStringAt(x + (int)idxName, y, row.text);
    dc.SetColor(foreground, background);
    dc.DrawStringAt(x + (int)idxRest, y, text.substr(idxRest));
    return true;
}

bool RelationshipModal::DrawSourceRow(DrawContext &dc, int x, int y, int width, const RelationshipTree::Row &row) {
    auto it = sourceLines.find({row.path, row.line});
    if ((it == sourceLines.end()) || (row.text.size() < it->second->Length())) {
        return false;
    }
    auto line = it->second;
    auto location = row.text.substr(0, row.text.size() - line->Length());
    auto xCode = x + (int)location.size();
    if (xCode >= width) {
        return false;       // no room for the code - the plain row is cut to fit
    }
    dc.DrawStringAt(x, y, location);
    LineRender(dc).DrawLine(xCode, y, line->CopyOfRange(0, (size_t)(width - xCode)));
    return true;
}

std::string RelationshipModal::RowText(const RelationshipTree::Row &row) const {
    auto text = UnicodeHelper::utf32to8(RelationshipTree::Label(row));
    if (!row.path.empty() && !row.isSource) {
        CodeIndex::Hit hit;
        hit.path = row.path;
        hit.line = row.line;
        text += "  " + SymbolSearchModal::HitLocation(hit, roots);
    }
    return text;
}

bool RelationshipModal::OnAction(const EditorAction &kpAction) {
    if (kpAction.uiAction == kUIAction::kUIActionCommitLine) {
        auto &row = GetTree()->GetCurrentSelectedItem();
        if (!row.path.empty()) {
            committed = row;
            Close();
        }
        return true;
    }
    return TreeSelectionModal::OnAction(kpAction);
}

void RelationshipModal::OnClosed(bool wasCancelled) {
    if (!wasCancelled && committed.has_value()) {
        ActionHelper::JumpTo(committed->path, committed->line, committed->column);
    }
}

gedit::Rect RelationshipModal::ComputeRect(const gedit::Rect &screenRect) {
    return CenterIn(screenRect, (screenRect.Width() * 7) / 10, (screenRect.Height() * 7) / 10);
}

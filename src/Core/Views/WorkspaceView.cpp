//
// Created by gnilk on 09.05.23.
//

#include "Core/Editor.h"
#include "WorkspaceView.h"


using namespace gedit;

static void FillTreeView(WorkspaceView::TreeRef tree, WorkspaceView::TreeNodeRef parent, Workspace::Node::Ref node) {
    std::vector<Workspace::Node::Ref> children;
    node->FlattenChilds(children);
    if (children.size() > 0) {
        for (auto &child: children) {
            auto newParent = tree->AddItem(parent, child);
            FillTreeView(tree, newParent, child);
        }
    }
    auto models = node->GetModels();
    if (models.size() > 0) {
        for(auto &nodeModel : models) {
            tree->AddItem(parent, nodeModel);
        }
    }

}

void WorkspaceView::InitView() {
    VisibleView::InitView();

    treeView = TreeView<Workspace::Node::Ref>::Create();
    treeView->SetToStringDelegate([this](Workspace::Node::Ref node) -> std::string {
        if (node->GetModel() != nullptr) {
            return std::string(node->GetModel()->GetTextBuffer()->GetFileName());
        }
        return node->GetName();
    });

    auto workspace = Editor::Instance().GetWorkspace();
    auto node = workspace.GetRootNode();
    auto item = treeView->AddItem(node);
    FillTreeView(treeView, item, node);

    // Traverse and add items

    // TODO: this should have a VStackView and a Header...
    AddView(treeView.get());
}
void WorkspaceView::ReInitView() {
    VisibleView::ReInitView();
}

bool WorkspaceView::OnAction(const KeyPressAction &kpAction) {
    if (treeView->OnAction(kpAction)) {
        return true;
    }
    if (kpAction.action == kAction::kActionCommitLine) {
        auto logger = gnilk::Logger::GetLogger("WorkspaceView");
        auto itemSelected = treeView->GetCurrentSelectedItem();
        if (itemSelected->GetModel() != nullptr) {
            logger->Debug("Selected Item: %s", itemSelected->GetModel()->GetTextBuffer()->GetFileName().c_str());
        } else {
            logger->Debug("You selected a directory!");
        }

    }
    return false;
}

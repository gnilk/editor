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
    PopulateTree();
    auto workspace = Editor::Instance().GetWorkspace();
    // Repopulate the tree on changes...
    workspace->SetChangeDelegate([this](){
       PopulateTree();
    });

    // TODO: this should have a VStackView and a Header...
    AddView(treeView.get());
}

void WorkspaceView::ReInitView() {
    VisibleView::ReInitView();
}

void WorkspaceView::PopulateTree() {
    if (treeView == nullptr) {
        treeView = TreeView<Workspace::Node::Ref>::Create();

        treeView->SetToStringDelegate([this](Workspace::Node::Ref node) -> std::string {
            if (node->GetModel() != nullptr) {
                return std::string(node->GetModel()->GetTextBuffer()->GetName());
            }
            return node->GetName();
        });
    } else {
        treeView->Clear();
    }


    auto workspace = Editor::Instance().GetWorkspace();

    // Traverse and add items
    auto nodes = workspace->GetRootNodes();
    for(auto &[key, node] : nodes) {
        auto item = treeView->AddItem(node);
        FillTreeView(treeView, item, node);
    }

}


bool WorkspaceView::OnAction(const KeyPressAction &kpAction) {
    if (treeView->OnAction(kpAction)) {
        return true;
    }
    if (kpAction.action == kAction::kActionCommitLine) {
        auto logger = gnilk::Logger::GetLogger("WorkspaceView");
        auto itemSelected = treeView->GetCurrentSelectedItem();
        if (itemSelected->GetModel() != nullptr) {
            Editor::Instance().OpenModelFromWorkspace(itemSelected);
            logger->Debug("Selected Item: %s", itemSelected->GetModel()->GetTextBuffer()->GetName().c_str());
            InvalidateAll();
            return true;
        } else {
            logger->Debug("You selected a directory!");
        }

    }
    return false;
}
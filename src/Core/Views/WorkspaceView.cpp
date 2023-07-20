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
    // Note: We take a COPY here - not a reference (auto &) - this allows for sorting...
    auto models = node->GetModels();
    if (models.size() > 0) {
        // Sort the models within
        std::sort(models.begin(), models.end(), [](Workspace::Node::Ref a, Workspace::Node::Ref b) -> bool {
            std::string strA = (a->GetModel() != nullptr) ? std::string(a->GetModel()->GetTextBuffer()->GetName()) : a->GetDisplayName();
            std::string strB = (b->GetModel() != nullptr) ? std::string(b->GetModel()->GetTextBuffer()->GetName()) : b->GetDisplayName();

            return (strA < strB);
        });
        // Now print them..
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

    AddView(treeView.get());
}

void WorkspaceView::ReInitView() {
    VisibleView::ReInitView();
}

void WorkspaceView::PopulateTree() {
    if (treeView == nullptr) {
        treeView = TreeView<Workspace::Node::Ref>::Create();

        treeView->SetToStringDelegate([](Workspace::Node::Ref node) -> std::string {
            if (node->GetModel() != nullptr) {
                return std::string(node->GetModel()->GetTextBuffer()->GetName());
            }
            return node->GetDisplayName();
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
    if (kpAction.action == kAction::kActionStartSearch) {
        auto logger = gnilk::Logger::GetLogger("WorkspaceView");
        logger->Debug("Start Searching!");
    }

    return false;
}

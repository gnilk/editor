//
// Created by gnilk on 09.05.23.
//

#include "Core/Editor.h"
#include "WorkspaceView.h"


using namespace gedit;

static const std::string cfgSectionName = "workspaceview";

static bool IsStringExcluded(const std::string &str, const std::vector<std::string> &excludePrefixes) {
    if (excludePrefixes.empty()) return false;
    for(auto &prefix : excludePrefixes) {
        if (strutil::startsWith(str, prefix)) {
            return true;
        }
    }
    return false;
}

static void FillTreeView(WorkspaceView::TreeRef tree, WorkspaceView::TreeNodeRef parent, Workspace::Node::Ref node, const std::vector<std::string> &excludePrefixes) {
    std::vector<Workspace::Node::Ref> children;
    node->FlattenChilds(children);
    if (children.size() > 0) {

        // Sort based on child nodes - this makes directories being on top...
        std::sort(children.begin(), children.end(), [](const Workspace::Node::Ref &a, const Workspace::Node::Ref &b) {
            return a->GetNumChildNodes() > b->GetNumChildNodes();
        });

        for (auto &child: children) {
            // This is probably not right
            if (IsStringExcluded(child->GetDisplayName(), excludePrefixes)) {
                continue;
            }
            auto newParent = tree->AddItem(parent, child);
            FillTreeView(tree, newParent, child,excludePrefixes);
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

    std::vector<std::string> excludePrefixList;
    // Perhaps have a list somewhere...
    if (Config::Instance()[cfgSectionName].GetBool("hide_dot_files", true)) {
        excludePrefixList.push_back(".");
    }

    // Traverse and add items
    auto nodes = workspace->GetRootNodes();
    for(auto &[key, node] : nodes) {
        auto item = treeView->AddItem(node);
        FillTreeView(treeView, item, node, excludePrefixList);
    }
    // All nodes start collapsed, but we want the root to start expanded...
    treeView->Expand();

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

void WorkspaceView::OnActivate(bool isActive) {
    if (isActive) {
        Editor::Instance().SetActiveKeyMapping(Config::Instance()[cfgSectionName].GetStr("keymap", "default_keymap"));
    }
}

std::pair<std::string, std::string> WorkspaceView::GetStatusBarInfo() {
    std::string strCenter = "apakaka";
    std::string strRight = {};

    auto node = treeView->GetCurrentSelectedItem();
    auto dispName = node->GetDisplayName();
    auto model = node->GetModel();
    if (model != nullptr) {
        dispName = model->GetTextBuffer()->GetName();
    }

    char tmp[32];
    snprintf(tmp, 32, "%s:%s",
             model==nullptr?"D":"F",
             dispName.c_str());
    strRight = tmp;
    return {strCenter, strRight};
}

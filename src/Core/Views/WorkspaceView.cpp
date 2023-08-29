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
                return std::string(node->GetDisplayName());
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
    auto currentItem = treeView->GetCurrentSelectedItem();
    workspace->SetActiveFolderNode(currentItem);

}


bool WorkspaceView::OnAction(const KeyPressAction &kpAction) {
    auto idxPrevActiveLine = treeView->idxActiveLine;
    if (treeView->OnAction(kpAction)) {
        if (idxPrevActiveLine != treeView->idxActiveLine) {
            auto activeNode = treeView->GetCurrentSelectedItem();
            auto nodeType = activeNode->GetMeta<int>(Workspace::Node::kMetaKey_NodeType, Workspace::Node::kNodeFolder);
            if (nodeType == Workspace::Node::kNodeFolder) {
                auto workspace = Editor::Instance().GetWorkspace();
                workspace->SetActiveFolderNode(activeNode);
            }
            // node did change!
            int breakme = 1;
        }
        return true;
    }
    if (kpAction.action == kAction::kActionCommitLine) {
        auto logger = gnilk::Logger::GetLogger("WorkspaceView");
        auto itemSelected = treeView->GetCurrentSelectedItem();
        if (itemSelected->GetModel() != nullptr) {
            Editor::Instance().OpenModelFromWorkspace(itemSelected);
            logger->Debug("Selected Item: %s", itemSelected->GetDisplayName().c_str());
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
    if (node == nullptr) {
        int breakme = 1;
    }
    auto &dispName = node->GetDisplayName();
    auto model = node->GetModel();

    auto nodeType = node->GetMeta<int>(Workspace::Node::kMetaKey_NodeType, Workspace::Node::kNodeFolder);
    auto fileSize = node->GetMeta<size_t>(Workspace::Node::kMetaKey_FileSize, 0);

    char tmp[32];
    if (nodeType == Workspace::Node::kNodeFolder) {
        snprintf(tmp, 32, "%s : <dir>", dispName.c_str());
    } else if (nodeType == Workspace::Node::kNodeFileRef) {
        snprintf(tmp, 32, "%s : %zu", dispName.c_str(), fileSize);
    } else {
        snprintf(tmp, 32, "---");
    }

    strRight = tmp;
    return {strCenter, strRight};
}

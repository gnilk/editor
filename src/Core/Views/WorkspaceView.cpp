//
// Created by gnilk on 09.05.23.
//

#include "Core/Editor.h"
#include "RootView.h"
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

static void FillTreeView(WorkspaceView::TreeRef tree, WorkspaceView::TreeNodeRef parent, Workspace::Node::Ref node, const std::vector<std::string> &excludePrefixes, const std::unordered_map<std::string, bool> &expandCollapseCache) {
    std::vector<Workspace::Node::Ref> children;

    auto itExpandCollapse = expandCollapseCache.find(node->GetNodePath().string());
    if (itExpandCollapse != expandCollapseCache.end()) {
        parent->isExpanded = itExpandCollapse->second;
    }


    node->FlattenChilds(children);
    if (children.size() > 0) {

        // Sort based on child nodes - this makes directories being on top...
        std::sort(children.begin(), children.end(), [](const Workspace::Node::Ref &a, const Workspace::Node::Ref &b) -> bool {
            bool aIsFolder = a->IsFolder();
            bool bIsFolder = b->IsFolder();

            if (aIsFolder && !bIsFolder)
                return true;

            if (!aIsFolder && bIsFolder)
                return false;

            // b Must be folder...
            if (aIsFolder)
                return (strcasecmp(a->GetDisplayName().c_str(), b->GetDisplayName().c_str()) < 0);

            return (strcasecmp(a->GetDisplayName().c_str(), b->GetDisplayName().c_str()) < 0);
        });



        for (auto &child: children) {
            // This is probably not right
            if (IsStringExcluded(child->GetDisplayName(), excludePrefixes)) {
                continue;
            }
            auto newParent = tree->AddItem(parent, child);
            FillTreeView(tree, newParent, child,excludePrefixes, expandCollapseCache);
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

    // When active buffer is changed, the re-init view is called
    // resync IF the user wants the buffers in workspace view to reflect the editor (this is VSCode behaviour)
    if (Config::Instance()[cfgSectionName].GetBool("sync_on_active_buffer_changed", true)) {
        if (Editor::Instance().GetActiveModel() != nullptr) {
            auto activeNode = Editor::Instance().GetWorkspaceNodeForActiveModel();
            treeView->SetCurrentlySelectedItem(activeNode);
        }
    }
}

WorkspaceView::TreeNodeRef WorkspaceView::FindModelNode(TreeNodeRef node, const std::string &pathName) {
    auto workspaceNode = node->data;
    if (workspaceNode->GetNodePath() == pathName) {
        return node;
    }
    for (auto &child : node->children) {
        auto res = FindModelNode(child, pathName);
        if (res != nullptr) {
            return res;
        }
    }
    return nullptr;
}
static void BuildExpandCollapseCacheFromNode(const WorkspaceView::TreeNodeRef &node, std::unordered_map<std::string, bool> &cache) {
    auto workspaceNode = node->data;
    if ((workspaceNode != nullptr) && (node->isExpanded)) {
        auto path = workspaceNode->GetNodePath();
        cache[path.string()] = node->isExpanded;
    }
    for(auto &child : node->children) {
        BuildExpandCollapseCacheFromNode(child, cache);
    }
}
void WorkspaceView::BuildExpandCollapseCache(std::unordered_map<std::string, bool> &cache) {
    BuildExpandCollapseCacheFromNode(treeView->GetRootNode(), cache);
}
void WorkspaceView::PopulateTree() {

    std::unordered_map<std::string, bool> expandCollapseCache;

    if (treeView == nullptr) {
        treeView = TreeView<Workspace::Node::Ref>::Create();

        treeView->SetToStringDelegate([](Workspace::Node::Ref node) -> std::string {
            if (node->GetModel() != nullptr) {
                return std::string(node->GetDisplayName());
            }
            // Highlight folders with '/'
            return (node->GetDisplayName() + "/");
        });
    } else {
        BuildExpandCollapseCache(expandCollapseCache);
        treeView->Clear();
    }

    auto workspace = Editor::Instance().GetWorkspace();

    std::vector<std::string> excludePrefixList;
    // Perhaps have a list somewhere...
    if (Config::Instance()[cfgSectionName].GetBool("hide_dot_files", true)) {
        excludePrefixList.push_back(".");
    }

    auto isFolderMonitorEnabled = Config::Instance()["foldermonitor"].GetBool("enabled", true);

    // Traverse and add items
    auto desktops = workspace->GetDesktops();
    for(auto &[key, desktop] : desktops) {
        auto rootNode = desktop->GetRootNode();
        auto treeRoot = treeView->AddItem(rootNode);

        if (isFolderMonitorEnabled) {
            desktop->StartFolderMonitor();
        }

        // TODO: We can add to exclude list from the Desktop->FolderMonitor->ExcludeList
        FillTreeView(treeView, treeRoot, rootNode, excludePrefixList, expandCollapseCache);
    }
    // All nodes start collapsed, but we want the root to start expanded...
    treeView->Expand();
    if (Editor::Instance().GetActiveModel() != nullptr) {
        auto activeNode = Editor::Instance().GetWorkspaceNodeForActiveModel();
        treeView->SetCurrentlySelectedItem(activeNode);
    }

    auto currentItem = treeView->GetCurrentSelectedItem();
    workspace->SetActiveFolderNode(currentItem);
}

bool WorkspaceView::OnAction(const KeyPressAction &kpAction) {
    auto &lineCursor = treeView->GetLineCursor();
    auto idxPrevActiveLine = lineCursor.idxActiveLine;

    if (treeView->OnAction(kpAction)) {
        if (idxPrevActiveLine != lineCursor.idxActiveLine) {
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

            if (Config::Instance()[cfgSectionName].GetBool("switch_to_editor_on_openfile", true)) {
                SwitchToEditorView();
            }

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

void WorkspaceView::SwitchToEditorView() {
    auto &rvBase = RuntimeConfig::Instance().GetRootView();
    RootView *rootView = static_cast<RootView *>(&rvBase);
    if (rootView == nullptr) {
        return;
    }
    rootView->SetActiveTopViewByName(glbEditorView);
}

void WorkspaceView::OnActivate(bool isActive) {
    if (isActive) {
        Editor::Instance().SetActiveKeyMapping(Config::Instance()[cfgSectionName].GetStr("keymap", "default_keymap"));
    }
}

std::pair<std::u32string, std::u32string> WorkspaceView::GetStatusBarInfo() {
    std::u32string strCenter = U"apakaka";
    std::u32string strRight = {};

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

    UnicodeHelper::ConvertUTF8ToUTF32String(strRight, tmp);

    return {strCenter, strRight};
}

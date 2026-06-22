//
// Created by gnilk on 09.05.23.
//

#include <filesystem>
#include "Core/Editor.h"
#include "Core/UI/Views/RootView.h"
#include "WorkspaceView.h"

namespace fs = std::filesystem;


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
            // FS-6 W5: the ONLY place where Workspace::Node::isScanned is translated into the
            // view flag. !is_symlink keeps non-followed symlink dirs as leaves (consistent with
            // scan behaviour — a symlink dir is emitted but not descended).
            newParent->hasUnfetchedChildren = child->IsFolder() && !child->IsScanned()
                                              && !fs::is_symlink(child->GetNodePath());
            FillTreeView(tree, newParent, child, excludePrefixes, expandCollapseCache);
        }
    }
}

void WorkspaceView::InitView() {
    VisibleView::InitView();
    CreateTree();
    AddView(treeView.get());

    PopulateTree();
    auto workspace = Editor::Instance().GetWorkspace();
    // Repopulate the tree on changes...
    workspace->SetChangeDelegate([this](){
       PopulateTree();
    });

}

// Note: Should add 'reason' (or action) perhaps to why this was called - we called for a number of reasons right now
// - active buffer changes
// - window resize
// - view resize
// - etc..
// Actually: 'InvalidateAll' and 'Reinitialize' is the 'goto' redraw mechanism (since I have no clue how the UI work anymore)
void WorkspaceView::ReInitView() {
    VisibleView::ReInitView();


    // Re-Init is called by a lot of reasons...
    // resync IF the user wants the buffers in workspace view to reflect the editor (this is VSCode behaviour)

    bool syncOnBufferChange = Config::Instance()[cfgSectionName].GetBool("sync_on_active_buffer_changed", true);
    if (syncOnBufferChange && Editor::Instance().GetActiveDocument() != nullptr) {
        auto activeNode = Editor::Instance().GetWorkspaceNodeForActiveDocument();
        treeView->SetCurrentlySelectedItem(activeNode);
    }


}

WorkspaceView::TreeNodeRef WorkspaceView::FindDocumentNode(TreeNodeRef node, const std::string &pathName) {
    auto workspaceNode = node->data;
    if (workspaceNode->GetNodePath() == pathName) {
        return node;
    }
    for (auto &child : node->children) {
        auto res = FindDocumentNode(child, pathName);
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

void WorkspaceView::SaveExpandCollapseState(std::unordered_map<std::string, bool> &out) {
    if (treeView == nullptr) {
        return;
    }
    BuildExpandCollapseCache(out);
}

void WorkspaceView::RestoreExpandCollapseState(const std::unordered_map<std::string, bool> &state) {
    pendingExpandCollapseSeed = state;
    hasPendingExpandCollapseSeed = true;
    // If the tree already exists (folder already open), re-apply now; otherwise the seed waits for the
    // first PopulateTree.
    if (treeView != nullptr) {
        PopulateTree();
    }
}

void WorkspaceView::CreateTree() {
    if (treeView == nullptr) {
        treeView = TreeView<Workspace::Node::Ref>::Create();

        treeView->SetToStringDelegate([](Workspace::Node::Ref node) -> std::string {
            // Highlight folders with '/'
            if (node->IsFolder()) {
                return (node->GetDisplayName() + "/");
            }
            return node->GetDisplayName();
        });

        // FS-6 W5: on expand of a node with hasUnfetchedChildren, run a shallow ScanNode
        // to populate the model, then mirror the new level into the view tree node.
        // No full PopulateTree — the cursor stays put and only this branch is updated.
        treeView->cbFetchChildrenForNode = [this](TreeNodeRef viewNode) {
            auto workspace = Editor::Instance().GetWorkspace();
            workspace->ScanNode(viewNode->data);

            std::vector<std::string> excludePrefixes;
            if (Config::Instance()[cfgSectionName].GetBool("hide_dot_files", true)) {
                excludePrefixes.push_back(".");
            }
            // Empty expand/collapse cache: freshly-scanned nodes have no saved state.
            FillTreeView(treeView, viewNode, viewNode->data, excludePrefixes, {});
        };
    }
}

// Must call 'CreateTree' before...
void WorkspaceView::PopulateTree() {

    std::unordered_map<std::string, bool> expandCollapseCache;

    BuildExpandCollapseCache(expandCollapseCache);
    // Merge a one-shot session seed (restored expand/collapse) over the live-tree snapshot, then clear it
    // so subsequent user collapse/expand is preserved on later rebuilds.
    if (hasPendingExpandCollapseSeed) {
        for (const auto &entry : pendingExpandCollapseSeed) {
            expandCollapseCache[entry.first] = entry.second;
        }
        hasPendingExpandCollapseSeed = false;
        pendingExpandCollapseSeed.clear();
    }
    treeView->Clear();

    auto workspace = Editor::Instance().GetWorkspace();

    std::vector<std::string> excludePrefixList;
    // Perhaps have a list somewhere...
    if (Config::Instance()[cfgSectionName].GetBool("hide_dot_files", true)) {
        excludePrefixList.push_back(".");
    }

    auto isFolderMonitorEnabled = Config::Instance()["foldermonitor"].GetBool("enabled", true);

    // Traverse and add items
    auto projectRoots = workspace->GetProjectRoots();
    for(auto &projectRoot : projectRoots) {
        auto rootNode = projectRoot->GetRootNode();
        auto treeRoot = treeView->AddItem(rootNode);

        if (isFolderMonitorEnabled) {
            projectRoot->StartFolderMonitor();
        }

        // TODO: We can add to exclude list from the ProjectRoot->FolderMonitor->ExcludeList
        FillTreeView(treeView, treeRoot, rootNode, excludePrefixList, expandCollapseCache);
    }
    // All nodes start collapsed, but we want the root to start expanded...
    treeView->Expand();
    if (Editor::Instance().GetActiveDocument() != nullptr) {
        auto activeNode = Editor::Instance().GetWorkspaceNodeForActiveDocument();
        treeView->SetCurrentlySelectedItem(activeNode);
    }

    auto currentItem = treeView->GetCurrentSelectedItem();
    workspace->SetActiveFolderNode(currentItem);


    if (Config::Instance()[cfgSectionName].GetBool("auto_expand_view", true)) {
        //auto clipWidth = Config::Instance()[cfgSectionName].GetInt("view_max_width", 0);
        // 0 - no clip width
        // treeView->ExpandViewToWidestItem();
    }
}

bool WorkspaceView::OnAction(const EditorAction &kpAction) {
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
        }
        return true;
    }
    if (kpAction.uiAction == kUIAction::kUIActionCommitLine) {
        auto logger = gnilk::Logger::GetLogger("WorkspaceView");
        auto itemSelected = treeView->GetCurrentSelectedItem();
        // File nodes are opened (document created lazily); folder nodes are not. Check the node type
        // rather than presence of a document - scanned file nodes are path-only until opened.
        auto nodeType = itemSelected->GetMeta<int>(Workspace::Node::kMetaKey_NodeType, Workspace::Node::kNodeFolder);
        if (nodeType != Workspace::Node::kNodeFolder) {
            Editor::Instance().OpenDocumentFromWorkspace(itemSelected);
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
        return true;
    }

    // Not for us - send further down the chain
    return ViewBase::OnAction(kpAction);
}

namespace {
    // Lines moved per wheel notch - mirrors EditorView's hardcoded "SIMPLE" first cut
    // (docs/mouse-support.md "Open questions": not yet keymap-configurable).
    constexpr int kWheelLinesPerNotch = 3;
}

// Active-line change side-effect shared by keyboard nav (OnAction) and mouse nav: if the newly
// selected node is a folder, it becomes the workspace's active folder node.
static void NotifyActiveFolderNodeIfChanged(WorkspaceView::TreeRef treeView) {
    auto activeNode = treeView->GetCurrentSelectedItem();
    auto nodeType = activeNode->GetMeta<int>(Workspace::Node::kMetaKey_NodeType, Workspace::Node::kNodeFolder);
    if (nodeType == Workspace::Node::kNodeFolder) {
        auto workspace = Editor::Instance().GetWorkspace();
        workspace->SetActiveFolderNode(activeNode);
    }
}

bool WorkspaceView::OnMouseEvent(const MouseEvent &mouseEvent) {
    if (treeView == nullptr) {
        return false;
    }

    switch (mouseEvent.kind) {
        case MouseEvent::kMouseEventKind_Press:
            return OnMousePressedEvent(mouseEvent);
        case MouseEvent::kMouseEventKind_Wheel:
            treeView->MoveSelectionByRows(-mouseEvent.wheelDelta * kWheelLinesPerNotch);
            NotifyActiveFolderNodeIfChanged(treeView);
            InvalidateAll();
            return true;
        default:
            return false;
    }
}

bool WorkspaceView::OnMousePressedEvent(const MouseEvent &mouseEvent) {
    // Rows are drawn through treeView's own window (TreeView::DrawViewContents), not this
    // wrapper's - so the click must be measured against treeView's content origin, not ours.
    auto contentOrigin = treeView->GetContentRect().TopLeft();
    int row = mouseEvent.y - contentOrigin.y;

    auto &lineCursor = treeView->GetLineCursor();
    treeView->SetSelectionAtRow(lineCursor.viewTopLine + row);
    NotifyActiveFolderNodeIfChanged(treeView);
    InvalidateAll();
    return true;
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
    auto document = node->GetDocument();

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

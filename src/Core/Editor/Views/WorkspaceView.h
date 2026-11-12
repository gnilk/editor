//
// Created by gnilk on 09.05.23.
//

#ifndef EDITOR_WORKSPACEVIEW_H
#define EDITOR_WORKSPACEVIEW_H

#include "Core/UI/Views/VisibleView.h"
#include "Core/UI/Views/TreeView.h"

#include "Core/Workspace.h"

namespace gedit {
    class WorkspaceView : public VisibleView {
    public:
        using Tree = TreeView<Workspace::Node::Ref>;
        using TreeRef = TreeView<Workspace::Node::Ref>::Ref;
        using TreeNodeRef = TreeView<Workspace::Node::Ref>::TreeNode::Ref;
    public:
        WorkspaceView() = default;
        virtual ~WorkspaceView() = default;

        void InitView() override;
        void ReInitView() override;

        void SetViewRect(const Rect &rect) override {
            if (treeView != nullptr) {
                treeView->SetViewRect(rect);
            }
        }

        void SetVisible(bool newIsVisible) override {
            parentView->SetVisible(newIsVisible);
        }
        bool IsVisible() override {
            return parentView->IsVisible();
        }

        void HandleKeyPress(const KeyPress &keyPress) override;
        bool OnAction(const EditorAction &kpAction) override;
        bool OnMouseEvent(const MouseEvent &mouseEvent) override;

        const std::u32string &GetStatusBarAbbreviation() override {
            static std::u32string defaultAbbr = U"WSP";
            return defaultAbbr;
        }
        std::pair<std::u32string, std::u32string> GetStatusBarInfo() override;

        // Session: persist/restore the tree expand/collapse state (docs/session-cache.md §9/§11.3).
        // Takes the path->expanded map directly (decoupled from RootSession, and avoiding a name clash
        // with ViewBase's layout ToSession/FromSession). Save snapshots the live tree; Restore stores a
        // one-shot seed applied on the next tree (re)build - so a restored node can still be collapsed.
        void SaveExpandCollapseState(std::unordered_map<std::string, bool> &out);
        void RestoreExpandCollapseState(const std::unordered_map<std::string, bool> &state);
    protected:
        TreeNodeRef FindDocumentNode(TreeNodeRef node, const std::string &pathName);
        void CreateTree();
        static std::string ChangeLetter(GitStatus::kChange change);
        static std::optional<ColorRGBA> ChangeColour(GitStatus::kChange change);
        void PopulateTree();
        bool SelectAfterRebuild(Workspace::Node::Ref prevSelected);
        TreeRef GetTreeView() {
            return treeView;
        }
        void BuildExpandCollapseCache(std::unordered_map<std::string, bool> &cache);
        void OnActivate(bool isActive) override;
        void SwitchToEditorView();
        bool OnMousePressedEvent(const MouseEvent &mouseEvent);
        // Open the selected node (file -> open + optional switch-to-editor; folder -> expand/collapse). Shared
        // by the Enter (OnAction) and double-click (OnMousePressedEvent) paths. True if a file opened or a
        // folder toggled.
        bool OpenSelectedItem();

    private:
        TreeRef treeView;
        // Cached from config ("workspaceview.lines_per_scroll_wheel_notch") in InitView/ReInitView.
        int linesPerScrollWheelNotch = 3;
        // A pause in typing this long ends a name ('workspaceview.type_to_find_pause_ms')
        int typeToFindPauseMs = 1000;
        // One-shot expand/collapse seed from a restored session: merged into the next PopulateTree then
        // cleared. Kept separate from the per-rebuild live-tree snapshot so it never overrides a later
        // user collapse/expand.
        std::unordered_map<std::string, bool> pendingExpandCollapseSeed;
        bool hasPendingExpandCollapseSeed = false;
        // Ignored items in the tree ('workspaceview.show_ignored', then ToggleIgnored). Survives a reinit.
        bool isShowingIgnored = true;
        // The buffer the selection last followed - compared, never dereferenced
        const Document *syncedDocument = nullptr;
    };
}


#endif //EDITOR_WORKSPACEVIEW_H

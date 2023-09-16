//
// Created by gnilk on 09.05.23.
//

#ifndef EDITOR_WORKSPACEVIEW_H
#define EDITOR_WORKSPACEVIEW_H

#include "VisibleView.h"
#include "TreeView.h"

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

        bool OnAction(const KeyPressAction &kpAction) override;

        const std::u32string &GetStatusBarAbbreviation() override {
            static std::u32string defaultAbbr = U"WSP";
            return defaultAbbr;
        }
        std::pair<std::u32string, std::u32string> GetStatusBarInfo() override;
    protected:
        TreeNodeRef FindModelNode(TreeNodeRef node, const std::string &pathName);
        void PopulateTree();
        void BuildExpandCollapseCache(std::unordered_map<std::string, bool> &cache);
        void OnActivate(bool isActive) override;
        void SwitchToEditorView();

    private:
        TreeRef treeView;
    };
}


#endif //EDITOR_WORKSPACEVIEW_H

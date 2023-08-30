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

        const std::string &GetStatusBarAbbreviation() override {
            static std::string defaultAbbr = "WSP";
            return defaultAbbr;
        }
        std::pair<std::string, std::string> GetStatusBarInfo() override;
    protected:
        TreeNodeRef FindModelNode(TreeNodeRef node, const std::string &pathName);
        void PopulateTree();
        void OnActivate(bool isActive) override;

    private:
        TreeRef treeView;
    };
}


#endif //EDITOR_WORKSPACEVIEW_H

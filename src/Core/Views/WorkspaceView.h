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

        void SetViewRect(const Rect &rect) {
            if (treeView != nullptr) {
                treeView->SetViewRect(rect);
            }
        }

        bool OnAction(const KeyPressAction &kpAction) override;

    private:
        TreeRef treeView;
    };
}


#endif //EDITOR_WORKSPACEVIEW_H

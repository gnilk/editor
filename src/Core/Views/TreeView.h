//
// Created by gnilk on 16.04.23.
//

#ifndef EDITOR_TREEVIEW_H
#define EDITOR_TREEVIEW_H

#include <memory>
#include <functional>
#include "VisibleView.h"
#include "Core/Editor.h"
#include "Core/VerticalNavigationViewModel.h"

namespace gedit {

    //
    // Generic tree view - you MUST call 'SetToStringDelegate' with a function that converts your data to a std::string
    //

    //
    // Note to self: ANY modification to the tree MUST call 'Flatten' in order for the change to be reflected during drawing!!!!
    //

    template<typename T>
    class TreeView : public VisibleView, public VerticalNavigationViewModel {
    public:
        struct TreeNode {
            using Ref = std::shared_ptr<TreeNode>;
            T data = {};
            Ref parent = nullptr;
            bool isExpanded = false;

            int indent = 0;

            std::vector<Ref> children = {};

            void Clear() {
                if (children.size() != 0) {
                    for(auto &child : children) {
                        child->Clear();
                    }
                    children.clear();
                }
                data = {};
            }

            static Ref Create(const T &itemData) {
                auto treeItem = std::make_shared<TreeNode>();
                treeItem->data = itemData;
                return treeItem;
            }
        };
    public:
        using Ref = std::shared_ptr<TreeView>;
        using ToStringDelegate = std::function<std::string(const T &data)>;
    public:
        TreeView() {
            rootNode = TreeNode::Create({});
        }
        virtual ~TreeView() = default;
        void InitView() override {
            VisibleView::InitView();
            rootNode->isExpanded = false;
            viewTopLine = 0;
            viewBottomLine = viewRect.Height();
        }
        void ReInitView() override {
            VisibleView::ReInitView();
            viewTopLine = 0;
            viewBottomLine = viewRect.Height();
        }

        const T &GetCurrentSelectedItem() {
            if (idxActiveLine > flattenNodeList.size()) {
                Flatten();
                if (idxActiveLine > flattenNodeList.size()) {
                    idxActiveLine = 0;
                }
            }
            return flattenNodeList[idxActiveLine]->data;
        }

        void Clear() {
            if (rootNode != nullptr) {
                rootNode->Clear();
            }
            flattenNodeList.clear();
        }

        const auto &GetRootNode() {
            return rootNode;
        }

        bool OnAction(const KeyPressAction &kpAction) override {
            bool wasHandled = true;
            switch(kpAction.action) {
                case kAction::kActionLineLeft :
                    Collapse();
                    break;
                case kAction::kActionLineRight :
                    Expand();
                    break;
                case kAction::kActionLineUp :
                    OnNavigateUpCLion(cursor, 1, GetContentRect(), flattenNodeList.size());
                    break;
                case kAction::kActionLineDown :
                    OnNavigateDownCLion(cursor, 1, GetContentRect(), flattenNodeList.size());
                    break;
                case kAction::kActionPageUp :
                    OnNavigateUpCLion(cursor, GetContentRect().Height()-1, GetContentRect(), flattenNodeList.size());
                    break;
                case kAction::kActionPageDown :
                    OnNavigateDownCLion(cursor, GetContentRect().Height()-1, GetContentRect(), flattenNodeList.size());
                    break;
                default:
                    wasHandled = false;
                    break;
            }
            if (!wasHandled) {
                return false;
            }
            InvalidateAll();
            return true;
        }

        void SetToStringDelegate(ToStringDelegate newToString) {
            cbToString = newToString;
        }

        typename TreeNode::Ref AddItem(const T &item) {
            return AddItem(rootNode, item);
        }

        typename TreeNode::Ref AddItem(typename TreeNode::Ref parent, const T &item) {
            auto treeItem = TreeNode::Create(item);
            //parent->isExpanded = false;
            parent->children.emplace_back(treeItem);
            treeItem->parent = parent;

            // Flatten tree!!!
            Flatten();

            return treeItem;
        }

        static Ref Create() {
            return std::make_shared<TreeView<T> >();
        }
        void Collapse() {
            auto &node = flattenNodeList[idxActiveLine];
            node->isExpanded = false;
            Flatten();
        }
        void Expand() {
            auto &node = flattenNodeList[idxActiveLine];
            node->isExpanded = true;
            Flatten();
        }


        bool SetCurrentlySelectedItem(const T &item) {
            // 1) we need to find the item in the tree
            auto node = FindNodeForItem(rootNode, item);
            if (node == nullptr) {
                return false;
            }
            // 2) expand all nodes leading up to that item
            ExpandToNode(node);
            // 3) flatten

            Flatten();
            // 4) find index of node in flatten list
            for(size_t i = 0; i < flattenNodeList.size();i++) {
                if (flattenNodeList[i]->data == item) {
                    // 5) update idxActiveLine with index from '4'
                    idxActiveLine = i;
                    return true;
                }
            }
            idxActiveLine = 0;
            return false;
        }

    protected:

        typename TreeNode::Ref FindNodeForItem(typename TreeNode::Ref node, const T &item) {
            if(node->data == item) {
                return node;
            }
            for(auto &child : node->children) {
                auto nodeWithItem = FindNodeForItem(child, item);
                if (nodeWithItem != nullptr) {
                    return nodeWithItem;
                }
            }
            return nullptr;
        }
        // Expand tree up to the node (this is down by expanding downwards)
        void ExpandToNode(typename TreeNode::Ref node) {
            node->isExpanded = true;
            if (node->parent != nullptr) {
                ExpandToNode(node->parent);
            }
        }
        // Note: Depends on flattening
        void DrawViewContents() override {
            auto &dc = window->GetContentDC();
            dc.ResetDrawColors();

            auto theme = Editor::Instance().GetTheme();
            auto &uiColors = theme->GetUIColors();
            dc.SetColor(uiColors["foreground"], uiColors["background"]);


            for(auto i=viewTopLine;i<viewBottomLine;i++) {
                if (i >= flattenNodeList.size()) {
                    break;
                }
                int yPos = i - viewTopLine;
                auto &node = flattenNodeList[i];
                auto str = cbToString(node->data);

                if (node->children.size() > 0) {
                    if (node->isExpanded) {
                        str = "-" + str;
                    } else {
                        str = "+" + str;
                    }
                } else {
                    str = " " + str;
                }
                dc.FillLine(yPos, kTextAttributes::kNormal, ' ');
                if (i == idxActiveLine) {
                    dc.FillLine(yPos, kTextAttributes::kNormal | kTextAttributes::kInverted, ' ');
                    dc.DrawStringWithAttributesAt(node->indent, yPos, kTextAttributes::kNormal | kTextAttributes::kInverted, str.c_str());

                } else {
                    dc.DrawStringWithAttributesAt(node->indent, yPos, kTextAttributes::kNormal, str.c_str());
                }
            }
        }

        //
        // This will flatten the tree to a vector making it way easier to handle during drawing...
        //
        void Flatten() {
            int idxLine = 0;
            flattenNodeList.clear();
            for(auto &node : rootNode->children) {
                idxLine = FlattenFromNode(idxLine, node, 0);
            }
        }

        int FlattenFromNode(int idxLine, typename TreeNode::Ref node, int indent) {
            node->indent = indent;
            flattenNodeList.emplace_back(node);
            idxLine += 1;
            if (node->isExpanded) {
                for (auto &child: node->children) {
                    idxLine = FlattenFromNode(idxLine, child, indent+2);
                }
            }
            return idxLine;
        }

    private:
        std::vector<typename TreeNode::Ref> flattenNodeList;
        ToStringDelegate cbToString = nullptr;
        typename TreeNode::Ref rootNode;

    };

}

#endif //EDITOR_TREEVIEW_H

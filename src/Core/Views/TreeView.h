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
    class TreeView : public VisibleView {
    public:
        struct TreeNode {
            using Ref = std::shared_ptr<TreeNode>;
            T data = {};
            Ref parent = nullptr;
            bool isExpanded = false;

            // this one is clipped and mutiliated during drawing - should now be relied on for anything
            std::string drawString = {};    // filled in during 'Flatten'

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
            verticalNavigationViewModel.lineCursor = &treeLineCursor;
            treeLineCursor.viewTopLine = 0;
            treeLineCursor.viewBottomLine = viewRect.Height();

        }
        void ReInitView() override {
            VisibleView::ReInitView();
            verticalNavigationViewModel.lineCursor = &treeLineCursor;
            treeLineCursor.viewTopLine = 0;
            treeLineCursor.viewBottomLine = viewRect.Height();
            // This will recompute the draw-strings
            Flatten();
        }

        const T &GetCurrentSelectedItem() {
            if (treeLineCursor.idxActiveLine > flattenNodeList.size()) {
                Flatten();
                if (treeLineCursor.idxActiveLine > flattenNodeList.size()) {
                    treeLineCursor.idxActiveLine = 0;
                }
            }
            return flattenNodeList[treeLineCursor.idxActiveLine]->data;
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
                    verticalNavigationViewModel.OnNavigateUp(1, GetContentRect(), flattenNodeList.size());
                    break;
                case kAction::kActionLineDown :
                    verticalNavigationViewModel.OnNavigateDown(1, GetContentRect(), flattenNodeList.size());
                    break;
                case kAction::kActionPageUp :
                    verticalNavigationViewModel.OnNavigateUp(GetContentRect().Height()-1, GetContentRect(), flattenNodeList.size());
                    break;
                case kAction::kActionPageDown :
                    verticalNavigationViewModel.OnNavigateDown(GetContentRect().Height()-1, GetContentRect(), flattenNodeList.size());
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
            auto &node = flattenNodeList[treeLineCursor.idxActiveLine];
            node->isExpanded = false;
            Flatten();
        }
        void Expand() {
            auto &node = flattenNodeList[treeLineCursor.idxActiveLine];
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
                    treeLineCursor.idxActiveLine = i;
                    return true;
                }
            }
            treeLineCursor.idxActiveLine = 0;
            return false;
        }

        LineCursor &GetLineCursor() {
            return treeLineCursor;
        }

        void ExpandViewToWidestItem() {
            if (flattenNodeList.empty()) return;

            int widthMax = 0;

            for(auto &node : flattenNodeList) {
                auto strWidth = node->drawString.length() + node->indent;
                if (strWidth > widthMax) {
                    widthMax = strWidth;
                }
            }

            // This can only happen when we are rendering
            auto lhandler = GetLayoutHandler();
            if (lhandler == nullptr) return;
            lhandler->SetWidth(widthMax);

            // If this is called during first initialization we don't have the root view yet..
            if (RuntimeConfig::Instance().HasRootView()) {
                RuntimeConfig::Instance().GetRootView().Initialize();
                RuntimeConfig::Instance().GetRootView().InvalidateAll();
            }
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

        void AdjustNodeDrawStrings() {
            auto widthMax = viewRect.Width();
            for(auto &node : flattenNodeList) {
                auto strWidth = node->drawString.length() + node->indent;

                if (strWidth < widthMax) continue;

                // erase last three chars and replace with '...'
                // sanity check for short strings
                if (strWidth > (node->indent + 6)) {
                    node->drawString.erase(widthMax - node->indent - 3);
                    node->drawString += "...";
                }
            }
        }

        // Note: Depends on flattening
        void DrawViewContents() override {
            auto &dc = window->GetContentDC();
            dc.ResetDrawColors();

            AdjustNodeDrawStrings();

            auto theme = Editor::Instance().GetTheme();
            auto &uiColors = theme->GetUIColors();
            dc.SetColor(uiColors["foreground"], uiColors["background"]);

            for(auto i=treeLineCursor.viewTopLine;i<treeLineCursor.viewBottomLine;i++) {
                if (i >= flattenNodeList.size()) {
                    break;
                }
                int yPos = i - treeLineCursor.viewTopLine;
                auto &node = flattenNodeList[i];
                auto &str = node->drawString;


                dc.FillLine(yPos, kTextAttributes::kNormal, ' ');
                if (i == treeLineCursor.idxActiveLine) {
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

            if (str == " file_with_very_long_name_should_be_ui_issue.txt") {
                int breakme = 1;
            }

            node->drawString = str;


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
        LineCursor treeLineCursor;  // This is not really used - just for storage
        VerticalNavigationCLion verticalNavigationViewModel;

    };

}

#endif //EDITOR_TREEVIEW_H

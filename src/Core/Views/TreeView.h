//
// Created by gnilk on 16.04.23.
//

#ifndef EDITOR_TREEVIEW_H
#define EDITOR_TREEVIEW_H

#include <memory>
#include <functional>
#include "VisibleView.h"
#include "Core/VerticalNavigationViewModel.h"

namespace gedit {
    template<typename T>
    class TreeView : public VisibleView, public VerticalNavigationViewModel {
    public:
        struct TreeItem {
            using Ref = std::shared_ptr<TreeItem>;
            T data = {};
            Ref parent = nullptr;
            std::vector<Ref> children = {};

            static Ref Create(const T &itemData) {
                auto treeItem = std::make_shared<TreeItem>();
                treeItem->data = itemData;
                return treeItem;
            }
        };
    public:
        using Ref = std::shared_ptr<TreeView>;
        using ToStringDelegate = std::function<std::string(const T &data)>;
    public:
        TreeView() = default;
        virtual ~TreeView() = default;
        void InitView() override {
            VisibleView::InitView();
            viewTopLine = 0;
            viewBottomLine = viewRect.Height();
        }
        void ReInitView() override {
            VisibleView::ReInitView();
            viewTopLine = 0;
            viewBottomLine = viewRect.Height();
        }

        bool OnAction(const KeyPressAction &kpAction) {
            return false;
        }

        void SetToStringDelegate(ToStringDelegate newToString) {
            cbToString = newToString;
        }

        typename TreeItem::Ref AddItem(const T &item) {
            auto treeItem = TreeItem::Create(item);
            rootNode.children.push_back(treeItem);
            return treeItem;
        }
        typename TreeItem::Ref AddItem(typename TreeItem::Ref parent, const T &item) {
            auto treeItem = TreeItem::Create(item);
            parent->children.push_back(treeItem);
            return treeItem;
        }

        static Ref Create() {
            return std::make_shared<TreeView<T> >();
        }
    protected:
        void DrawViewContents() override {
            auto &dc = window->GetContentDC();
            std::vector<std::string> treeAsVector;
            Flatten(treeAsVector, rootNode, 0);
            for (int i = viewTopLine; i < viewBottomLine; i++) {
                if (i > treeAsVector.size()) {
                    break;
                }
                int yPos = i - viewTopLine;
                dc.DrawStringWithAttributesAt(0, yPos, kTextAttributes::kNormal, treeAsVector[i].c_str());
            }
        }
    private:
        void Flatten(std::vector<std::string> &outVector, const TreeItem &node, int depth) {
            for(int i=0;i<node.children.size();i++) {
                auto strData = cbToString(node.children[i]->data);
                outVector.push_back(strData);
            }
        }
    private:
        ToStringDelegate cbToString = nullptr;
        TreeItem rootNode;

    };

}

#endif //EDITOR_TREEVIEW_H

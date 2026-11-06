//
// Created by gnilk on 16.04.23.
//

#ifndef EDITOR_TREESELECTIONMODAL_H
#define EDITOR_TREESELECTIONMODAL_H

#include <vector>
#include <string>
#include <memory>

#include "ModalView.h"
#include "Core/Graphics/Cursor.h"
#include "Core/VerticalNavigationViewModel.h"
#include "Core/UI/Views/VStackView.h"

#include "Core/UI/Views/TreeView.h"

namespace gedit {
    template <typename T>
    class TreeSelectionModal : public ModalView {
    public:
        TreeSelectionModal() {
            treeView = TreeView<T>::Create();
        }
        explicit TreeSelectionModal(const Rect & rect) : ModalView(rect) {
            treeView = TreeView<T>::Create();
        }
        virtual ~TreeSelectionModal() = default;

        void InitView() override {
            ModalView::InitView();
            layoutView = std::make_unique<VStackView>(GetContentRect());
            AddView(layoutView.get());
            layoutView->AddSubView(treeView.get(), kFill);
        }
        bool OnAction(const EditorAction &kpAction) override {
            if (treeView->OnAction(kpAction)) {
                return true;
            }
            return ModalView::OnAction(kpAction);
        }

        typename TreeView<T>::Ref GetTree() {
            return treeView;
        }

    private:
        std::unique_ptr<VStackView> layoutView = nullptr;
        typename TreeView<T>::Ref treeView;
    };
}


#endif //EDITOR_TREESELECTIONMODAL_H

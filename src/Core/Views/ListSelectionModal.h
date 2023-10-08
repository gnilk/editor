//
// Created by gnilk on 14.04.23.
//

#ifndef EDITOR_LISTSELECTIONMODAL_H
#define EDITOR_LISTSELECTIONMODAL_H

#include <vector>
#include <string>
#include "ModalView.h"
#include "Core/Cursor.h"
#include "Core/VerticalNavigationViewModel.h"
#include "Core/Views/VStackView.h"


namespace gedit {


    class ListView : public ViewBase, public VerticalNavigationViewModel {
    public:
        ListView() = default;
        virtual ~ListView() = default;
        // These two functions should be in a 'VisibleView' or 'DrawableView'
        // As they are duplicated quite a lot...
        void InitView() override;
        void ReInitView() override;
        bool OnAction(const KeyPressAction &kpAction) override;


        void SetListItems(const std::vector<std::string> &newItems) {
            listItems = newItems;
        }
        void AddItem(const std::string &item) {
            listItems.push_back(item);
        }

        const std::vector<std::string> &GetAllItems() {
            return listItems;
        }
        int GetSelectedItemIndex() {
            return listLineCursor.idxActiveLine;
        }
        void SetSelectedItem(int newSelection) {
            if (newSelection < 0) {
                newSelection = 0;
            } else if (newSelection > ((int)listItems.size()-1)) {
                newSelection = listItems.size()-1;
            }
            listLineCursor.idxActiveLine = newSelection;
        }
        const std::string &GetSelectedItem() {
            return listItems[listLineCursor.idxActiveLine];
        }
    protected:
        void DrawViewContents() override;
    private:
        LineCursor listLineCursor;
        std::vector<std::string> listItems;

    };

    class ListSelectionModal : public ModalView {
    public:
        ListSelectionModal() {
            listView = new ListView();
        }
        explicit ListSelectionModal(const Rect & rect) : ModalView(rect) {
            listView = new ListView();
        }
        virtual ~ListSelectionModal() = default;

        void InitView() override;

        bool OnAction(const KeyPressAction &kpAction) override;

        std::optional<int> GetSelectedItemIndex();
        const std::string &GetSelectedItem();
        void SetListItems(std::vector<std::string> &newItems);
        void AddItem(const std::string &strItem);
    protected:
        void DrawViewContents() override;
    protected:
        int idxSelectedItem = -1;
        VStackView *layoutView;
        ListView *listView;
    };
}


#endif //EDITOR_LISTSELECTIONMODAL_H

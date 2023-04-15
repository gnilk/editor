//
// Created by gnilk on 14.04.23.
//

#include "ListSelectionModal.h"
#include "VStackView.h"
#include "SingleLineView.h"


using namespace gedit;
class ListHeaderView : public SingleLineView {
public:
    ListHeaderView() = default;
    virtual ~ListHeaderView() = default;
protected:
    void DrawViewContents() override {
        auto &dc = window->GetContentDC();
        dc.FillLine(0, kTextAttributes::kInverted, ' ');
        dc.DrawStringWithAttributesAt(0,0,kTextAttributes::kNormal | kTextAttributes::kInverted, "mamma was here");
    }
};

//
// Move this to own file...
//

// These two functions should be in a 'VisibleView' or 'DrawableView'
// As they are duplicated quite a lot...
void ListView::InitView() {
    auto screen = RuntimeConfig::Instance().Screen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("ListView");
}
void ListView::ReInitView() {
    auto screen = RuntimeConfig::Instance().Screen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
}

bool ListView::OnAction(const KeyPressAction &kpAction){
    if (kpAction.action == kAction::kActionLineUp) {
        SetSelectedItem(GetSelectedItemIndex()-1);
        return true;
    } else if (kpAction.action == kAction::kActionLineDown) {
        SetSelectedItem(GetSelectedItemIndex()+1);
        return true;
    }
    return false;
}


void ListView::DrawViewContents() {
    auto &dc = window->GetContentDC();
    int lineCount = 0;
    for(auto &s : listItems) {
        if (lineCount == idxSelectedLine) {
            dc.FillLine(lineCount, kTextAttributes::kNormal | kTextAttributes::kInverted, ' ');
            dc.DrawStringWithAttributesAt(0,lineCount,kTextAttributes::kNormal  | kTextAttributes::kInverted, s.c_str());
        } else {
            dc.DrawStringWithAttributesAt(0, lineCount, kTextAttributes::kNormal, s.c_str());
        }
        lineCount++;
    }
}
////////////////
//
//

void ListSelectionModal::InitView() {
    ModalView::InitView();

    auto layoutView = new VStackView(viewRect);
    AddView(layoutView);

    layoutView->AddSubView(new ListHeaderView(), kFixed);
    layoutView->AddSubView(listView, kFill);
}

bool ListSelectionModal::OnAction(const KeyPressAction &kpAction) {
    if (listView->OnAction(kpAction)) {
        return true;
    }

    if (kpAction.action == kAction::kActionCommitLine) {
        listView->GetSelectedItemIndex();
        CloseModal();
    }
    return ModalView::OnAction(kpAction);
}

std::optional<int> ListSelectionModal::GetSelectedItemIndex() {
    if (idxSelectedItem == -1) {
        return {};
    }
    return idxSelectedItem;
}
const std::string &ListSelectionModal::GetSelectedItem() {
    return listView->GetSelectedItem();
}


void ListSelectionModal::SetListItems(std::vector<std::string> &newItems) {
    listView->SetListItems(newItems);
}
void ListSelectionModal::AddItem(const std::string &strItem) {
    listView->AddItem(strItem);
}
void ListSelectionModal::DrawViewContents() {
//    auto &dc = window->GetContentDC();
//    dc.DrawStringWithAttributesAt(0,0,kTextAttributes::kNormal, "baseclass");
}


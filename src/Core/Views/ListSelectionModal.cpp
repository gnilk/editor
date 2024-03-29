//
// Created by gnilk on 14.04.23.
// Simple list selection modal dialog
//

#include "ListSelectionModal.h"
#include "VStackView.h"
#include "SingleLineView.h"
#include "Core/Config/Config.h"
#include "Core/NamedColors.h"


using namespace gedit;
class ListHeaderView : public SingleLineView {
public:
    ListHeaderView() = default;
    virtual ~ListHeaderView() = default;
protected:
    void DrawViewContents() override {
        auto &dc = window->GetContentDC();
        dc.ResetDrawColors();
        dc.FillLine(0, kTextAttributes::kInverted, ' ');
        dc.DrawStringWithAttributesAndColAt(0,0,kTextAttributes::kNormal | kTextAttributes::kInverted, 0, "Header");
    }
};

//
// Move this to own file...
//

// These two functions should be in a 'VisibleView' or 'DrawableView'
// As they are duplicated quite a lot...
void ListView::InitView() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    verticalNavigationModel.lineCursor = &listLineCursor;
    listLineCursor.viewTopLine = 0;
    listLineCursor.viewBottomLine = viewRect.Height();
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("ListView");
}
void ListView::ReInitView() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    listLineCursor.viewTopLine = 0;
    listLineCursor.viewBottomLine = viewRect.Height();
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
}

bool ListView::OnAction(const KeyPressAction &kpAction){
    bool wasHandled = true;
    switch(kpAction.action) {
        case kAction::kActionLineUp :
            verticalNavigationModel.OnNavigateUp(1, GetContentRect(), listItems.size());
            break;
        case kAction::kActionLineDown :
            verticalNavigationModel.OnNavigateDown(1, GetContentRect(), listItems.size());
            break;
        case kAction::kActionPageUp :
            verticalNavigationModel.OnNavigateUp(GetContentRect().Height()-1, GetContentRect(), listItems.size());
            break;
        case kAction::kActionPageDown :
            verticalNavigationModel.OnNavigateDown(GetContentRect().Height()-1, GetContentRect(), listItems.size());
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

void ListView::DrawViewContents() {
    auto &dc = window->GetContentDC();
    dc.ResetDrawColors();
    for (auto i = listLineCursor.viewTopLine; i < listLineCursor.viewBottomLine; i++) {
        if (i >= listItems.size()) {
            break;
        }
        int yPos = i - listLineCursor.viewTopLine;
        auto &str = listItems[i];
        dc.FillLine(yPos, kTextAttributes::kNormal, ' ');
        if (i == listLineCursor.idxActiveLine) {
            dc.FillLine(yPos, kTextAttributes::kNormal | kTextAttributes::kInverted, ' ');
            dc.DrawStringWithAttributesAt(0,yPos,kTextAttributes::kNormal  | kTextAttributes::kInverted, str.c_str());
        } else {
            dc.DrawStringWithAttributesAt(0, yPos, kTextAttributes::kNormal, str.c_str());
        }
    }
}
////////////////
//
// This is the modal dialog box...
// It has a Vertical Stacked layout with a header (optional) and a listview..
//
static ListHeaderView *listHeader = nullptr;
void ListSelectionModal::InitView() {
    ModalView::InitView();

    layoutView = new VStackView(GetContentRect());
    AddView(layoutView);

    listHeader = new ListHeaderView();
    listHeader->SetVisible(false);
    layoutView->AddSubView(listHeader, kFixed);
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
    if (kpAction.action == kAction::kActionLineRight) {
        if (listHeader->IsVisible()) {
            listHeader->SetVisible(false);
        } else {
            listHeader->SetVisible(true);
        }
        layoutView->RecomputeLayout();
        InvalidateAll();
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


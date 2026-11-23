//
// See TabbedView.h
//

#include <algorithm>

#include "Core/UI/UIHost.h"
#include "TabbedView.h"

using namespace gedit;

ViewBase *TabbedView::AddTab(std::unique_ptr<ViewBase> newView, const std::u32string &label) {
    bool wasRowVisible = IsTabRowVisible();
    auto view = newView.get();
    tabs.push_back({std::move(newView), label});
    view->SetLayoutHandler(this);
    AddView(view);
    if (!activeTab.has_value()) {
        activeTab = 0;
        view->SetVisible(true);
        if (isActive) {
            view->SetActive(true);
        }
    } else {
        view->SetVisible(false);
    }
    LayoutTabs();
    // The second tab shows the row: the active tab moves down a row
    if (wasRowVisible != IsTabRowVisible()) {
        if (auto active = ActiveView()) {
            active->Initialize();
        }
    }
    InvalidateAll();
    return view;
}

void TabbedView::RemoveTab(size_t idxTab) {
    // Deleted here
    TakeTab(idxTab);
}

std::unique_ptr<ViewBase> TabbedView::TakeTab(size_t idxTab) {
    if (idxTab >= tabs.size()) {
        return nullptr;
    }
    auto view = tabs[idxTab].view.get();
    bool wasShown = (activeTab == idxTab);
    bool hadFocus = wasShown && isActive;
    // Still a tab here: losing the focus may lay the tabs out
    if (hadFocus) {
        view->SetActive(false);
    }
    view->SetVisible(false);
    // Handed back after the handler, which may still ask about the tabs
    auto removed = std::move(tabs[idxTab].view);
    tabs.erase(tabs.begin() + (long)idxTab);
    RemoveView(view);

    if (tabs.empty()) {
        activeTab.reset();
    } else if (wasShown) {
        // The one after it took its index; the last one's place goes to the one before
        activeTab = std::min(idxTab, tabs.size() - 1);
        ActiveView()->SetVisible(true);
    } else if (activeTab.value() > idxTab) {
        activeTab = activeTab.value() - 1;
    }
    // The row may have gone (one tab left): the active tab takes its row
    LayoutTabs();
    if (auto active = ActiveView()) {
        active->Initialize();
        if (hadFocus) {
            active->SetActive(true);
        }
    }
    InvalidateAll();
    if (wasShown && activeTab.has_value() && tabChangedHandler) {
        tabChangedHandler(activeTab.value());
    }
    return removed;
}

ViewBase *TabbedView::TabView(size_t idxTab) const {
    return (idxTab < tabs.size()) ? tabs[idxTab].view.get() : nullptr;
}

ViewBase *TabbedView::ActiveView() const {
    return activeTab.has_value() ? tabs[activeTab.value()].view.get() : nullptr;
}

std::optional<size_t> TabbedView::IndexOf(const ViewBase *view) const {
    for (size_t i = 0; i < tabs.size(); i++) {
        if (tabs[i].view.get() == view) {
            return i;
        }
    }
    return std::nullopt;
}

void TabbedView::SetActiveTab(size_t idxTab) {
    if ((idxTab >= tabs.size()) || (activeTab == idxTab)) {
        return;
    }
    bool hasFocus = isActive;
    if (auto previous = ActiveView()) {
        if (hasFocus) {
            previous->SetActive(false);
        }
        previous->SetVisible(false);
    }
    activeTab = idxTab;
    auto next = tabs[idxTab].view.get();
    next->SetVisible(true);
    LayoutTabs();
    next->Initialize();
    // Focus moves with it - its keymap installs
    if (hasFocus) {
        next->SetActive(true);
    }
    InvalidateAll();
    if (tabChangedHandler) {
        tabChangedHandler(idxTab);
    }
}

void TabbedView::NextTab() {
    if (!activeTab.has_value() || (tabs.size() < 2)) {
        return;
    }
    SetActiveTab((activeTab.value() + 1) % tabs.size());
}

void TabbedView::PreviousTab() {
    if (!activeTab.has_value() || (tabs.size() < 2)) {
        return;
    }
    SetActiveTab((activeTab.value() + tabs.size() - 1) % tabs.size());
}

void TabbedView::SetTabLabel(size_t idxTab, const std::u32string &label) {
    if ((idxTab >= tabs.size()) || (tabs[idxTab].label == label)) {
        return;
    }
    tabs[idxTab].label = label;
    if (IsTabRowVisible()) {
        InvalidateView();
    }
}

const std::u32string &TabbedView::TabLabel(size_t idxTab) const {
    static const std::u32string none;
    return (idxTab < tabs.size()) ? tabs[idxTab].label : none;
}

std::vector<TabbedView::TabSpan> TabbedView::LayoutTabRow(const std::vector<std::u32string> &labels) {
    std::vector<TabSpan> spans;
    int x = 1;
    for (size_t i = 0; i < labels.size(); i++) {
        if (i > 0) {
            x += (int)SEPARATOR.size();
        }
        spans.push_back({i, x, x + (int)labels[i].size()});
        x += (int)labels[i].size();
    }
    return spans;
}

//
// Top-view proxy
//
void TabbedView::HandleKeyPress(const KeyPress &keyPress) {
    if (auto active = ActiveView()) {
        active->HandleKeyPress(keyPress);
    }
}

bool TabbedView::OnAction(const EditorAction &action) {
    if (auto active = ActiveView(); (active != nullptr) && active->OnAction(action)) {
        return true;
    }
    return ViewBase::OnAction(action);
}

const std::u32string &TabbedView::GetStatusBarAbbreviation() {
    if (auto active = ActiveView()) {
        return active->GetStatusBarAbbreviation();
    }
    return ViewBase::GetStatusBarAbbreviation();
}

std::pair<std::u32string, std::u32string> TabbedView::GetStatusBarInfo() {
    if (auto active = ActiveView()) {
        return active->GetStatusBarInfo();
    }
    return ViewBase::GetStatusBarInfo();
}

// A press on a label shows that tab; the rest of the row does nothing
bool TabbedView::OnMouseEvent(const MouseEvent &mouseEvent) {
    if ((mouseEvent.kind != MouseEvent::kMouseEventKind_Press) || !IsTabRowVisible()) {
        return false;
    }
    auto content = GetContentRect();
    if (mouseEvent.y != content.TopLeft().y) {
        return false;
    }
    int x = mouseEvent.x - content.TopLeft().x;
    for (auto &span : LayoutTabRow(Labels())) {
        if ((x >= span.xStart) && (x < span.xEnd)) {
            SetActiveTab(span.idxTab);
            return true;
        }
    }
    return true;
}

void TabbedView::OnActionIncreaseWidth() {
    GetLayoutHandler()->OnActionIncreaseWidth();
}

void TabbedView::OnActionDecreaseWidth() {
    GetLayoutHandler()->OnActionDecreaseWidth();
}

void TabbedView::OnActionIncreaseHeight() {
    GetLayoutHandler()->OnActionIncreaseHeight();
}

void TabbedView::OnActionDecreaseHeight() {
    GetLayoutHandler()->OnActionDecreaseHeight();
}

void TabbedView::MaximizeContentHeight() {
    parentView->MaximizeContentHeight();
}

void TabbedView::RestoreContentHeight() {
    parentView->RestoreContentHeight();
}

void TabbedView::ResetContentHeight() {
    parentView->ResetContentHeight();
}

//
// View
//
void TabbedView::InitView() {
    auto screen = UIHost::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("TabbedView");
    LayoutTabs();
}

void TabbedView::ReInitView() {
    auto screen = UIHost::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    LayoutTabs();
}

void TabbedView::DrawViewContents() {
    if (!IsTabRowVisible()) {
        return;
    }
    auto &dc = window->GetContentDC();
    dc.ResetDrawColors();
    auto &uiColors = UIHost::Instance().GetUIColors();
    if (IsActive()) {
        dc.SetColor(uiColors["header_active_foreground"], uiColors["header_active_background"]);
    } else {
        dc.SetColor(uiColors["header_foreground"], uiColors["header_background"]);
    }
    dc.FillLine(0, kTextAttributes::kNormal, ' ');
    auto spans = LayoutTabRow(Labels());
    for (auto &span : spans) {
        if (span.idxTab > 0) {
            dc.DrawStringWithAttributesAt(span.xStart - (int)SEPARATOR.size(), 0, kTextAttributes::kNormal, SEPARATOR.c_str());
        }
        auto attributes = (activeTab == span.idxTab) ? kTextAttributes::kUnderline : kTextAttributes::kNormal;
        dc.DrawStringWithAttributesAt(span.xStart, 0, attributes, tabs[span.idxTab].label.c_str());
    }
}

void TabbedView::OnActivate(bool isNowActive) {
    if (auto active = ActiveView()) {
        active->SetActive(isNowActive);
    }
}

void TabbedView::LayoutTabs() {
    if (window == nullptr) {
        return;
    }
    auto rect = TabContentRect();
    for (auto &tab : tabs) {
        tab.view->SetViewRect(rect);
    }
}

Rect TabbedView::TabContentRect() {
    auto rect = GetContentRect();
    if (IsTabRowVisible() && (rect.Height() > 1)) {
        rect.MoveTo(rect.TopLeft().x, rect.TopLeft().y + 1);
        rect.SetHeight(rect.Height() - 1);
    }
    return rect;
}

std::vector<std::u32string> TabbedView::Labels() const {
    std::vector<std::u32string> labels;
    for (auto &tab : tabs) {
        labels.push_back(tab.label);
    }
    return labels;
}

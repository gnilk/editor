//
// See PanelHost.h
//

#include "Core/Editor.h"
#include "PanelHost.h"

using namespace gedit;

void PanelHost::Build(const Parts &newParts, const TerminalSession &session, const ConfigNode &config, const std::string &setting, const Rect &dimensions) {
    auto validSetting = PanelLayout::IsValidSetting(setting) ? setting : std::string("portrait");
    auto startLayout = PanelLayout::Choose(validSetting, dimensions.Width(), dimensions.Height());
    BuildFromPlan(newParts, PanelLayout::PlanTabs(session, config, startLayout), startLayout, validSetting);
}

void PanelHost::BuildFromPlan(const Parts &newParts, const PanelLayout::Plan &plan, PanelLayout::kLayout startLayout, const std::string &setting) {
    parts = newParts;
    layoutSetting = setting;
    layout = startLayout;

    // Ids name a tab's files - unique over both panels
    auto allIds = [this]() {
        return PanelView::IdsOf({&bottom, &right});
    };
    bottom.SetIdSource(allIds);
    bottom.SetTopViewName(glbTerminalView);
    right.SetIdSource(allIds);
    right.SetTopViewName(glbTerminalRightView);
    right.SetDefaultHome(PanelView::HOME_RIGHT);
    right.SetKeepsLastTerminal(false);

    bottom.AddTabs(plan.bottom, plan.activeBottom);
    right.AddTabs(plan.right, plan.activeRight);
    rightActiveId = plan.activeRight;

    parts.statusSplit->SetLower(&bottom);
    panelsSplit.SetInitialSplitterRatio(RIGHT_PANEL_SPLIT);
    panelsSplit.SetSessionId("split.panel_right");
    if (right.NumTabs() > 0) {
        ShowRight(true);
    } else {
        parts.statusSplit->SetUpper(parts.editorArea);
    }

    // auto follows the window - switched before the resize lays the tree out, so the old layout is never squeezed
    parts.rootView->SetScreenResizedHandler([this](const Rect &newDimensions) {
        OnScreenResized(newDimensions, true);
    });
}

void PanelHost::RegisterTopViews() {
    parts.rootView->AddTopView(&bottom, glbTerminalView);
    if (isRightShown) {
        parts.rootView->AddTopView(&right, glbTerminalRightView);
    }
    areTopViewsRegistered = true;
}

bool PanelHost::SetLayoutSetting(const std::string &setting) {
    if (!PanelLayout::IsValidSetting(setting)) {
        return false;
    }
    layoutSetting = setting;
    auto dimensions = parts.rootView->GetViewRect();
    Apply(PanelLayout::Choose(layoutSetting, dimensions.Width(), dimensions.Height()));
    return true;
}

void PanelHost::OnScreenResized(const Rect &dimensions, bool isLaidOutNext) {
    if (layoutSetting != "auto") {
        return;
    }
    auto newLayout = PanelLayout::Choose(layoutSetting, dimensions.Width(), dimensions.Height(), layout);
    if (newLayout != layout) {
        Apply(newLayout, isLaidOutNext);
    }
}

void PanelHost::SaveTabs(TerminalSession &out) {
    // In portrait the right panel's shown tab is the one it had - kept for the next landscape start
    out.activeTabIdRight = rightActiveId;
    PanelLayout::SaveTabs(bottom, Right(), out);
}

std::string PanelHost::MoveShownTab(const std::string &focusedTopView) {
    auto isFromRight = (focusedTopView == glbTerminalRightView) && isRightShown;
    if (!isFromRight && (focusedTopView != glbTerminalView)) {
        return "Focus a panel first (Opt/Alt+T) - the shown tab there moves";
    }
    auto &from = isFromRight ? right : bottom;
    auto idxTab = from.ActiveTab();
    if (!idxTab.has_value()) {
        return "No tab to move";
    }
    // The editor's own tab stays with the editor's things (agent-integration.md §6.2)
    if (from.IsEditorAgentTab(idxTab.value())) {
        return "The Editor Agent stays in the bottom panel";
    }
    auto home = from.TabHome(idxTab.value());
    auto newHome = (home == PanelView::HOME_RIGHT) ? PanelView::HOME_BOTTOM : PanelView::HOME_RIGHT;
    auto isTerminal = dynamic_cast<TerminalView *>(from.TabView(idxTab.value())) != nullptr;
    // The bottom panel keeps a terminal of its own - the build shell, the output console
    if ((home == PanelView::HOME_BOTTOM) && isTerminal && (bottom.NumTerminalsWithHome(PanelView::HOME_BOTTOM) <= 1)) {
        return "The bottom panel's last terminal stays";
    }
    from.SetTabHome(idxTab.value(), newHome);
    if (layout == PanelLayout::kLayout::kPortrait) {
        return (newHome == PanelView::HOME_RIGHT) ? "The tab belongs on the right now - shown there in landscape ('.layout landscape')"
                                                  : "The tab belongs in the bottom panel now";
    }
    auto &to = isFromRight ? bottom : right;
    auto moved = from.TakeTab(idxTab.value());
    if (!moved.has_value()) {
        return "No tab to move";
    }
    moved->wasShown = true;
    to.PutTab(std::move(moved.value()));
    if (!isFromRight) {
        ShowRight(true);
    } else if (right.NumTabs() == 0) {
        ShowRight(false);
    }
    Relayout();
    // Focus goes with the tab
    parts.rootView->SetActiveTopViewByName(isFromRight ? glbTerminalView : glbTerminalRightView);
    return isFromRight ? "Moved to the bottom panel" : "Moved to the right panel";
}

void PanelHost::Apply(PanelLayout::kLayout newLayout, bool isLaidOutNext) {
    if (newLayout == layout) {
        return;
    }
    layout = newLayout;
    if (layout == PanelLayout::kLayout::kLandscape) {
        auto toMove = bottom.TabsWithHome(PanelView::HOME_RIGHT);
        if (toMove.empty()) {
            return;
        }
        // The bottom panel keeps a terminal of its own (when it had one to lose)
        if ((bottom.Terminals().size() > 0) && (bottom.NumTerminalsWithHome(PanelView::HOME_BOTTOM) == 0)) {
            auto idxShown = bottom.ActiveTab();
            bottom.NewTerminal(PanelView::HOME_BOTTOM);
            if (idxShown.has_value()) {
                bottom.SetActiveTab(idxShown.value());
            }
        }
        MoveTabsWithHome(bottom, right, PanelView::HOME_RIGHT, true);
        if (rightActiveId.has_value()) {
            right.ShowTabId(rightActiveId.value());
        }
        ShowRight(true);
    } else {
        if (!isRightShown) {
            return;
        }
        rightActiveId = right.ActiveTabId();
        // The bottom panel's shown tab stays shown
        MoveTabsWithHome(right, bottom, PanelView::HOME_RIGHT, false);
        // A tab moved to the right while in landscape (its home changed) goes too
        MoveTabsWithHome(right, bottom, PanelView::HOME_BOTTOM, false);
        ShowRight(false);
    }
    if (!isLaidOutNext) {
        Relayout();
    }
}

void PanelHost::MoveTabsWithHome(PanelView &from, PanelView &to, const std::string &home, bool keepShown) {
    // Taken from the back, so the indices before stay; put in the order they had
    auto indices = from.TabsWithHome(home);
    std::vector<PanelView::MovedTab> moved;
    moved.reserve(indices.size());
    for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
        if (auto tab = from.TakeTab(*it)) {
            tab->wasShown = keepShown && tab->wasShown;
            moved.push_back(std::move(tab.value()));
        }
    }
    for (auto it = moved.rbegin(); it != moved.rend(); ++it) {
        to.PutTab(std::move(*it));
    }
}

void PanelHost::ShowRight(bool isShown) {
    if (isShown == isRightShown) {
        return;
    }
    isRightShown = isShown;
    if (isShown) {
        panelsSplit.SetLeft(parts.editorArea);
        panelsSplit.SetRight(&right);
        parts.statusSplit->SetUpper(&panelsSplit);
        if (areTopViewsRegistered) {
            parts.rootView->AddTopView(&right, glbTerminalRightView);
        }
        return;
    }
    // Focus doesn't stay on a panel that leaves
    if (parts.rootView->GetTopViewName() == glbTerminalRightView) {
        parts.rootView->SetActiveTopViewByName(glbTerminalView);
    }
    parts.rootView->RemoveTopView(glbTerminalRightView);
    parts.statusSplit->SetUpper(parts.editorArea);
}

void PanelHost::Relayout() {
    if (!parts.rootView->IsInitialized()) {
        return;
    }
    parts.rootView->Resize();
    parts.rootView->InvalidateAll();
    Editor::Instance().TriggerUIRedraw();
}

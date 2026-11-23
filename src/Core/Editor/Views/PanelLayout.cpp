//
// See PanelLayout.h
//

#include <algorithm>

#include "PanelLayout.h"

using namespace gedit;

PanelLayout::kLayout PanelLayout::Choose(const std::string &setting, int cols, int rows) {
    if (setting == "landscape") {
        return kLayout::kLandscape;
    }
    if ((setting == "auto") && (cols > 0) && (rows > 0)) {
        auto aspect = (float)cols / ((float)rows * CELL_ASPECT);
        return (aspect >= LANDSCAPE_ASPECT) ? kLayout::kLandscape : kLayout::kPortrait;
    }
    return kLayout::kPortrait;
}

PanelLayout::Plan PanelLayout::PlanTabs(const TerminalSession &session, const ConfigNode &config, kLayout layout) {
    std::vector<PanelView::RestoredTab> tabs;
    Plan plan;
    if (!session.tabs.empty()) {
        for (auto &tab : session.tabs) {
            PanelView::RestoredTab restored;
            restored.spec.id = tab.id;
            restored.spec.name = tab.name.empty() ? PanelView::NameFor(tab.id) : tab.name;
            restored.spec.view = tab.view.empty() ? PanelView::VIEW_TERMINAL : tab.view;
            restored.spec.config = tab.config.empty() ? restored.spec.view : tab.config;
            restored.spec.home = (tab.home == PanelView::HOME_RIGHT) ? PanelView::HOME_RIGHT : PanelView::HOME_BOTTOM;
            restored.scrollbackFile = tab.scrollbackFile;
            tabs.push_back(restored);
        }
        plan.activeBottom = session.activeTabId;
        plan.activeRight = session.activeTabIdRight;
        // A session from before ids were saved: the shown tab by index (every tab was in the bottom panel)
        if (!plan.activeBottom.has_value() && (session.activeTab > 0) && ((size_t)session.activeTab < tabs.size())) {
            plan.activeBottom = tabs[(size_t)session.activeTab].spec.id;
        }
    } else {
        // No tabs in the session (none yet, or one from before tabs - its one terminal's files go to the first tab)
        std::vector<std::string> ids;
        for (auto &[section, home] : {std::pair{"panel", PanelView::HOME_BOTTOM}, std::pair{"panel_right", PanelView::HOME_RIGHT}}) {
            for (auto &spec : PanelView::ConfiguredTabs(config, section, home, ids)) {
                tabs.push_back({spec, spec.id.empty() ? session.scrollbackFile : ""});
            }
        }
    }

    for (auto &tab : tabs) {
        auto isRight = (layout == kLayout::kLandscape) && (tab.spec.home == PanelView::HOME_RIGHT);
        (isRight ? plan.right : plan.bottom).push_back(tab);
    }

    // A terminal the bottom panel always has - the build shell, the output console
    auto hasTerminal = std::any_of(plan.bottom.begin(), plan.bottom.end(), [](const PanelView::RestoredTab &tab) {
        return tab.spec.view == PanelView::VIEW_TERMINAL;
    });
    if (!hasTerminal) {
        std::vector<std::string> ids;
        for (auto &tab : tabs) {
            ids.push_back(tab.spec.id);
        }
        auto isFirst = std::find(ids.begin(), ids.end(), std::string()) == ids.end();
        auto id = isFirst ? std::string() : PanelView::NextId(ids);
        PanelView::RestoredTab shell;
        shell.spec.id = id;
        shell.spec.name = PanelView::NameFor(id);
        shell.scrollbackFile = id.empty() ? session.scrollbackFile : "";
        plan.bottom.insert(plan.bottom.begin(), shell);
    }
    return plan;
}

void PanelLayout::SaveTabs(PanelView &bottom, PanelView *right, TerminalSession &out) {
    // A portrait run has no right panel - keep what it showed for the next landscape start
    auto keptRight = out.activeTabIdRight;
    out = TerminalSession{};
    bottom.CollectTabs(out.tabs);
    out.activeTab = (int32_t)bottom.ActiveTab().value_or(0);
    out.activeTabId = bottom.ActiveTabId();
    if (right != nullptr) {
        right->CollectTabs(out.tabs);
        out.activeTabIdRight = right->ActiveTabId();
    } else {
        out.activeTabIdRight = keptRight;
    }
    for (auto &tab : out.tabs) {
        if (tab.id.empty()) {
            out.scrollbackFile = tab.scrollbackFile;
        }
    }
}

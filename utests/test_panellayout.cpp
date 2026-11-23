//
// The panel layouts (hitlist #77, docs/usability-round.md §19): portrait / landscape / auto, each tab in its home's
// panel, ids unique over both, the session's homes and shown tabs.
//
#include <testinterface.h>

#include "Core/Config/ConfigNode.h"
#include "Core/Editor.h"
#include "Core/Editor/Views/PanelLayout.h"
#include "Core/Editor/Views/PanelView.h"
#include "Core/Session/SessionState.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_panellayout(ITesting *t);
DLL_EXPORT int test_panellayout_choose(ITesting *t);
DLL_EXPORT int test_panellayout_configured(ITesting *t);
DLL_EXPORT int test_panellayout_portrait(ITesting *t);
DLL_EXPORT int test_panellayout_session(ITesting *t);
DLL_EXPORT int test_panellayout_session_before(ITesting *t);
DLL_EXPORT int test_panellayout_bottom_terminal(ITesting *t);
DLL_EXPORT int test_panellayout_save(ITesting *t);
DLL_EXPORT int test_panellayout_newtab(ITesting *t);
}

static const char *CONFIG_BOTH =
    "panel:\n"
    "  tabs:\n"
    "    - name: Shell\n"
    "    - view: messages\n"
    "panel_right:\n"
    "  tabs:\n"
    "    - name: Claude\n"
    "      config: ai_agent\n";

static std::vector<std::string> IdsOf(const std::vector<PanelView::RestoredTab> &tabs) {
    std::vector<std::string> ids;
    for (auto &tab : tabs) {
        ids.push_back(tab.spec.id);
    }
    return ids;
}

// The views below draw on the headless screen
DLL_EXPORT int test_panellayout(ITesting *t) {
    Editor::Instance().OpenScreen();
    RuntimeConfig::Instance().SetMainThreadID();
    return kTR_Pass;
}

// portrait / landscape as asked; auto by the window's shape - wide is landscape, half a wide screen or upright is not;
// anything else (or no size yet) is portrait
DLL_EXPORT int test_panellayout_choose(ITesting *t) {
    using kLayout = PanelLayout::kLayout;
    TR_ASSERT(t, PanelLayout::Choose("portrait", 400, 50) == kLayout::kPortrait);
    TR_ASSERT(t, PanelLayout::Choose("landscape", 80, 100) == kLayout::kLandscape);
    // 1920x1080 at 8x16 cells: 240x67
    TR_ASSERT(t, PanelLayout::Choose("auto", 240, 67) == kLayout::kLandscape);
    // Half of it: 120x67
    TR_ASSERT(t, PanelLayout::Choose("auto", 120, 67) == kLayout::kPortrait);
    // An upright monitor: 135x120
    TR_ASSERT(t, PanelLayout::Choose("auto", 135, 120) == kLayout::kPortrait);
    TR_ASSERT(t, PanelLayout::Choose("auto", 0, 0) == kLayout::kPortrait);
    TR_ASSERT(t, PanelLayout::Choose("sideways", 240, 67) == kLayout::kPortrait);
    TR_ASSERT(t, PanelLayout::Choose("", 240, 67) == kLayout::kPortrait);
    return kTR_Pass;
}

// No session: panel.tabs at the bottom, panel_right.tabs on the right - ids continue over both, each tab knows its home;
// the first tab takes the files from before tabs
DLL_EXPORT int test_panellayout_configured(ITesting *t) {
    auto cfg = ConfigNode::FromString(CONFIG_BOTH);
    TR_ASSERT(t, cfg.has_value());
    TerminalSession session;
    session.scrollbackFile = "terminal_scrollback.bin";
    auto plan = PanelLayout::PlanTabs(session, cfg.value(), PanelLayout::kLayout::kLandscape);
    TR_ASSERT(t, plan.bottom.size() == 2);
    TR_ASSERT(t, plan.right.size() == 1);
    TR_ASSERT(t, plan.bottom[0].spec.id.empty() && (plan.bottom[0].scrollbackFile == "terminal_scrollback.bin"));
    TR_ASSERT(t, plan.bottom[1].spec.view == PanelView::VIEW_MESSAGES);
    TR_ASSERT(t, (plan.right[0].spec.id == "3") && (plan.right[0].spec.config == "ai_agent"));
    TR_ASSERT(t, plan.right[0].spec.home == PanelView::HOME_RIGHT);
    TR_ASSERT(t, plan.bottom[0].spec.home == PanelView::HOME_BOTTOM);
    TR_ASSERT(t, plan.right[0].scrollbackFile.empty());

    // No panel_right: no right panel, in either layout
    auto bottomOnly = ConfigNode::FromString("panel:\n  tabs:\n    - name: Shell\n");
    TR_ASSERT(t, PanelLayout::PlanTabs(session, bottomOnly.value(), PanelLayout::kLayout::kLandscape).right.empty());
    return kTR_Pass;
}

// Portrait: every tab in the bottom panel, the right-homed ones after - each still knows its home
DLL_EXPORT int test_panellayout_portrait(ITesting *t) {
    auto cfg = ConfigNode::FromString(CONFIG_BOTH);
    auto plan = PanelLayout::PlanTabs({}, cfg.value(), PanelLayout::kLayout::kPortrait);
    TR_ASSERT(t, plan.right.empty());
    TR_ASSERT(t, plan.bottom.size() == 3);
    TR_ASSERT(t, plan.bottom[2].spec.name == "Claude");
    TR_ASSERT(t, plan.bottom[2].spec.home == PanelView::HOME_RIGHT);
    return kTR_Pass;
}

// A session's tabs go back to their homes in landscape - whatever layout saved them - with each panel's shown tab
DLL_EXPORT int test_panellayout_session(ITesting *t) {
    auto none = ConfigNode::FromString("panel:\n  other: 1\n");
    TerminalSession session;
    session.tabs = {
        {"", "Shell", "terminal_scrollback.bin", "terminal", "terminal", "bottom"},
        {"2", "Claude", "", "terminal", "ai_agent", "right"},
        {"3", "Shell 3", "", "terminal", "terminal", "right"},
        {"4", "Messages", "", "messages", "messages", ""},
    };
    session.activeTabId = "4";
    session.activeTabIdRight = "3";
    auto plan = PanelLayout::PlanTabs(session, none.value(), PanelLayout::kLayout::kLandscape);
    TR_ASSERT(t, (IdsOf(plan.bottom) == std::vector<std::string>{"", "4"}));
    TR_ASSERT(t, (IdsOf(plan.right) == std::vector<std::string>{"2", "3"}));
    TR_ASSERT(t, plan.activeBottom == std::optional<std::string>("4"));
    TR_ASSERT(t, plan.activeRight == std::optional<std::string>("3"));
    TR_ASSERT(t, plan.bottom[0].scrollbackFile == "terminal_scrollback.bin");

    // The same session in portrait: all at the bottom, in the saved order
    auto portrait = PanelLayout::PlanTabs(session, none.value(), PanelLayout::kLayout::kPortrait);
    TR_ASSERT(t, portrait.right.empty());
    TR_ASSERT(t, (IdsOf(portrait.bottom) == std::vector<std::string>{"", "2", "3", "4"}));
    return kTR_Pass;
}

// A session from before #77: no homes (all bottom), the shown tab by index
DLL_EXPORT int test_panellayout_session_before(ITesting *t) {
    auto cfg = ConfigNode::FromString(CONFIG_BOTH);
    TerminalSession session;
    session.tabs = {{"", "Shell", ""}, {"2", "Shell 2", ""}, {"3", "Claude", "", "terminal", "ai_agent"}};
    session.activeTab = 2;
    // The session wins over the config - its tabs, not panel_right's
    auto plan = PanelLayout::PlanTabs(session, cfg.value(), PanelLayout::kLayout::kLandscape);
    TR_ASSERT(t, plan.right.empty());
    TR_ASSERT(t, plan.bottom.size() == 3);
    TR_ASSERT(t, plan.activeBottom == std::optional<std::string>("3"));
    return kTR_Pass;
}

// Landscape with every terminal homed right: the bottom panel still gets one (the output console), with a free id
DLL_EXPORT int test_panellayout_bottom_terminal(ITesting *t) {
    auto cfg = ConfigNode::FromString(
        "panel:\n"
        "  tabs:\n"
        "    - view: messages\n"
        "panel_right:\n"
        "  tabs:\n"
        "    - name: Claude\n");
    auto plan = PanelLayout::PlanTabs({}, cfg.value(), PanelLayout::kLayout::kLandscape);
    TR_ASSERT(t, plan.bottom.size() == 2);
    TR_ASSERT(t, plan.bottom[0].spec.view == PanelView::VIEW_TERMINAL);
    TR_ASSERT(t, plan.bottom[0].spec.id == "3");
    TR_ASSERT(t, (plan.right.size() == 1) && (plan.right[0].spec.id == "2"));

    // Nothing configured at all: one Shell with the first id
    auto none = ConfigNode::FromString("panel:\n  other: 1\n");
    auto bare = PanelLayout::PlanTabs({}, none.value(), PanelLayout::kLayout::kLandscape);
    TR_ASSERT(t, (bare.bottom.size() == 1) && bare.bottom[0].spec.id.empty() && (bare.bottom[0].spec.name == "Shell"));
    TR_ASSERT(t, bare.right.empty());
    return kTR_Pass;
}

// Saved: the bottom panel's tabs then the right's, each with its home and the shown ones; a portrait run (no right
// panel) keeps the right panel's shown tab from the session it started with
DLL_EXPORT int test_panellayout_save(ITesting *t) {
    // Messages tabs - no shell is started for them
    PanelView bottom;
    PanelView right;
    bottom.AddTabs({{{"", "Messages", PanelView::VIEW_MESSAGES, "messages", PanelView::HOME_BOTTOM}, ""},
                    {{"2", "Build", PanelView::VIEW_MESSAGES, "messages", PanelView::HOME_RIGHT}, ""}}, std::string("2"));
    right.AddTabs({{{"3", "Agent", PanelView::VIEW_MESSAGES, "messages", PanelView::HOME_RIGHT}, ""}}, std::string("3"));

    TerminalSession out;
    PanelLayout::SaveTabs(bottom, &right, out);
    TR_ASSERT(t, out.tabs.size() == 3);
    TR_ASSERT(t, (out.tabs[1].id == "2") && (out.tabs[1].home == PanelView::HOME_RIGHT));
    TR_ASSERT(t, (out.tabs[2].id == "3") && (out.tabs[2].home == PanelView::HOME_RIGHT));
    TR_ASSERT(t, out.tabs[0].home == PanelView::HOME_BOTTOM);
    TR_ASSERT(t, out.activeTabId == std::optional<std::string>("2"));
    TR_ASSERT(t, out.activeTab == 1);
    TR_ASSERT(t, out.activeTabIdRight == std::optional<std::string>("3"));

    // Portrait: no right panel - what the right one showed last time is kept
    out.activeTabIdRight = "7";
    PanelLayout::SaveTabs(bottom, nullptr, out);
    TR_ASSERT(t, out.tabs.size() == 2);
    TR_ASSERT(t, out.activeTabIdRight == std::optional<std::string>("7"));
    return kTR_Pass;
}

// Cmd+T: the new tab's id is free over both panels, and it belongs where the tab it was opened from does - a right-homed
// tab shown in portrait's bottom panel opens a right-homed one
DLL_EXPORT int test_panellayout_newtab(ITesting *t) {
    PanelView bottom;
    PanelView right;
    auto allIds = [&bottom, &right]() {
        return PanelView::IdsOf({&bottom, &right});
    };
    bottom.SetIdSource(allIds);
    right.SetIdSource(allIds);
    right.SetDefaultHome(PanelView::HOME_RIGHT);
    bottom.AddTabs({{{"", "Messages", PanelView::VIEW_MESSAGES, "messages", PanelView::HOME_BOTTOM}, ""},
                    {{"2", "Agent", PanelView::VIEW_MESSAGES, "messages", PanelView::HOME_RIGHT}, ""}}, std::string("2"));
    right.AddTabs({{{"3", "Other", PanelView::VIEW_MESSAGES, "messages", PanelView::HOME_RIGHT}, ""}}, {});

    bottom.NewTerminal();
    std::vector<TerminalTabSession> tabs;
    bottom.CollectTabs(tabs);
    TR_ASSERT(t, tabs.size() == 3);
    TR_ASSERT(t, tabs[2].id == "4");
    TR_ASSERT(t, tabs[2].home == PanelView::HOME_RIGHT);

    // From the bottom-homed tab: bottom
    bottom.SetActiveTab(0);
    bottom.NewTerminal();
    tabs.clear();
    bottom.CollectTabs(tabs);
    TR_ASSERT(t, (tabs.size() == 4) && (tabs[3].id == "5") && (tabs[3].home == PanelView::HOME_BOTTOM));
    return kTR_Pass;
}

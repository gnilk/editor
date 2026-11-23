//
// The panel layouts (hitlist #77, docs/usability-round.md §19): portrait / landscape / auto, each tab in its home's
// panel, ids unique over both, the session's homes and shown tabs.
//
#include <testinterface.h>

#include "Core/Config/ConfigNode.h"
#include "Core/Editor.h"
#include "Core/Editor/Views/PanelHost.h"
#include "Core/Editor/Views/PanelLayout.h"
#include "Core/UI/Views/HSplitView.h"
#include "Core/UI/Views/RootView.h"
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
DLL_EXPORT int test_panellayout_choose_margin(ITesting *t);
DLL_EXPORT int test_panellayout_switch(ITesting *t);
DLL_EXPORT int test_panellayout_move(ITesting *t);
DLL_EXPORT int test_panellayout_auto(ITesting *t);
DLL_EXPORT int test_panellayout_reparent(ITesting *t);
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

    // Only panel_right configured: the bottom's Shell takes the first id, the right panel's tab the next
    auto rightOnly = ConfigNode::FromString("panel_right:\n  tabs:\n    - name: Agent\n");
    auto planRight = PanelLayout::PlanTabs({}, rightOnly.value(), PanelLayout::kLayout::kLandscape);
    TR_ASSERT(t, (planRight.bottom.size() == 1) && planRight.bottom[0].spec.id.empty() && (planRight.bottom[0].spec.name == "Shell"));
    TR_ASSERT(t, (planRight.right.size() == 1) && (planRight.right[0].spec.id == "2"));

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

// auto while running: near the threshold the layout stays as it is - a window dragged across it doesn't flicker
DLL_EXPORT int test_panellayout_choose_margin(ITesting *t) {
    using kLayout = PanelLayout::kLayout;
    // 150x50 cells: 1.5 exactly - the start says landscape, a running one keeps what it has
    TR_ASSERT(t, PanelLayout::Choose("auto", 150, 50) == kLayout::kLandscape);
    TR_ASSERT(t, PanelLayout::Choose("auto", 150, 50, kLayout::kPortrait) == kLayout::kPortrait);
    TR_ASSERT(t, PanelLayout::Choose("auto", 145, 50, kLayout::kLandscape) == kLayout::kLandscape);
    // Well past it either way: it changes
    TR_ASSERT(t, PanelLayout::Choose("auto", 170, 50, kLayout::kPortrait) == kLayout::kLandscape);
    TR_ASSERT(t, PanelLayout::Choose("auto", 130, 50, kLayout::kLandscape) == kLayout::kPortrait);
    // Not auto: the setting
    TR_ASSERT(t, PanelLayout::Choose("portrait", 400, 50, kLayout::kLandscape) == kLayout::kPortrait);
    TR_ASSERT(t, PanelLayout::IsValidSetting("auto") && !PanelLayout::IsValidSetting("wide"));
    return kTR_Pass;
}

// A tree as main.cpp has it, with tabs that start no shell: the status split, the editor part above it
namespace {
    struct HostTree {
        RootView rootView;
        HSplitView statusSplit;
        ViewBase editorArea;
        PanelHost host;

        HostTree(const std::string &setting, PanelLayout::kLayout startLayout) {
            rootView.AddView(&statusSplit);
            PanelLayout::Plan plan;
            plan.bottom = {{{"", "Build", PanelView::VIEW_MESSAGES, "messages", PanelView::HOME_BOTTOM}, ""},
                           {{"2", "Agent", PanelView::VIEW_MESSAGES, "messages", PanelView::HOME_RIGHT}, ""}};
            if (startLayout == PanelLayout::kLayout::kLandscape) {
                plan.right = {plan.bottom[1]};
                plan.bottom.pop_back();
            }
            host.BuildFromPlan({&rootView, &statusSplit, &editorArea}, plan, startLayout, setting);
            rootView.AddTopView(&editorArea, glbEditorView);
            host.RegisterTopViews();
            rootView.Initialize();
        }
    };
}

// Portrait to landscape and back while running: the right-homed tab goes to a right panel beside the editor part (the
// whole upper height) and back - the same view, not a new one; the top view comes and goes with it
DLL_EXPORT int test_panellayout_switch(ITesting *t) {
    HostTree tree("portrait", PanelLayout::kLayout::kPortrait);
    auto &host = tree.host;
    TR_ASSERT(t, host.Right() == nullptr);
    TR_ASSERT(t, host.Bottom().NumTabs() == 2);
    auto agentView = host.Bottom().TabView(1);
    int width = tree.statusSplit.GetViewRect().Width();
    TR_ASSERT(t, tree.editorArea.GetViewRect().Width() == width);

    TR_ASSERT(t, host.SetLayoutSetting("landscape"));
    TR_ASSERT(t, host.Right() != nullptr);
    TR_ASSERT(t, (host.Bottom().NumTabs() == 1) && (host.Right()->NumTabs() == 1));
    TR_ASSERT(t, host.Right()->TabView(0) == agentView);
    TR_ASSERT(t, tree.rootView.GetTopViewByName(glbTerminalRightView) != nullptr);
    auto rightRect = host.Right()->GetViewRect();
    auto editorRect = tree.editorArea.GetViewRect();
    TR_ASSERT(t, editorRect.Width() + rightRect.Width() == width);
    TR_ASSERT(t, (rightRect.TopLeft().y == editorRect.TopLeft().y) && (rightRect.Height() == editorRect.Height()));
    TR_ASSERT(t, editorRect.Width() > 0 && rightRect.Width() > 0);

    // Focus in the right panel, then portrait: focus goes to the bottom panel, the tab with it
    TR_ASSERT(t, tree.rootView.SetActiveTopViewByName(glbTerminalRightView));
    TR_ASSERT(t, host.SetLayoutSetting("portrait"));
    TR_ASSERT(t, host.Right() == nullptr);
    TR_ASSERT(t, host.Bottom().NumTabs() == 2);
    TR_ASSERT(t, host.Bottom().TabView(1) == agentView);
    TR_ASSERT(t, tree.rootView.GetTopViewByName(glbTerminalRightView) == nullptr);
    TR_ASSERT(t, tree.rootView.GetTopViewName() == std::optional<std::string>(glbTerminalView));
    TR_ASSERT(t, tree.editorArea.GetViewRect().Width() == width);

    // A setting that isn't one: nothing changes
    TR_ASSERT(t, !host.SetLayoutSetting("sideways"));
    TR_ASSERT(t, host.LayoutSetting() == "portrait");
    return kTR_Pass;
}

// '.panel move': in landscape the tab moves (its home with it) and focus follows; the right panel goes when its last
// tab leaves and comes back when one is moved there; in portrait only the home changes
DLL_EXPORT int test_panellayout_move(ITesting *t) {
    HostTree tree("landscape", PanelLayout::kLayout::kLandscape);
    auto &host = tree.host;
    TR_ASSERT(t, host.Right() != nullptr);
    auto agentView = host.Right()->TabView(0);

    host.MoveShownTab(glbTerminalRightView);
    TR_ASSERT(t, host.Right() == nullptr);
    TR_ASSERT(t, (host.Bottom().NumTabs() == 2) && (host.Bottom().TabView(1) == agentView));
    TR_ASSERT(t, host.Bottom().TabHome(1) == PanelView::HOME_BOTTOM);
    TR_ASSERT(t, host.Bottom().ActiveTab() == std::optional<size_t>(1));
    TR_ASSERT(t, tree.rootView.GetTopViewName() == std::optional<std::string>(glbTerminalView));

    host.MoveShownTab(glbTerminalView);
    TR_ASSERT(t, (host.Right() != nullptr) && (host.Right()->TabView(0) == agentView));
    TR_ASSERT(t, host.Bottom().NumTabs() == 1);
    TR_ASSERT(t, tree.rootView.GetTopViewName() == std::optional<std::string>(glbTerminalRightView));

    // Not from a panel: nothing moves
    host.MoveShownTab(glbEditorView);
    TR_ASSERT(t, host.Right()->NumTabs() == 1);

    // Portrait: only the home - the tab stays where it is shown
    TR_ASSERT(t, host.SetLayoutSetting("portrait"));
    host.Bottom().SetActiveTab(1);
    host.MoveShownTab(glbTerminalView);
    TR_ASSERT(t, (host.Bottom().NumTabs() == 2) && (host.Bottom().TabHome(1) == PanelView::HOME_BOTTOM));
    // ... so landscape has no right panel now
    TR_ASSERT(t, host.SetLayoutSetting("landscape"));
    TR_ASSERT(t, host.Right() == nullptr);
    return kTR_Pass;
}

// auto follows the window: wide -> landscape, narrow -> portrait, near the threshold it stays; the session keeps the
// right panel's shown tab through a portrait spell. Called as the root's resize calls it - the switch only, the resize
// lays it out (the headless screen keeps its own size)
DLL_EXPORT int test_panellayout_auto(ITesting *t) {
    HostTree tree("auto", PanelLayout::kLayout::kPortrait);
    auto &host = tree.host;
    host.OnScreenResized(gedit::Rect(240, 67), true);
    TR_ASSERT(t, host.Right() != nullptr);
    host.OnScreenResized(gedit::Rect(150, 50), true);
    TR_ASSERT(t, host.Right() != nullptr);
    host.OnScreenResized(gedit::Rect(120, 67), true);
    TR_ASSERT(t, host.Right() == nullptr);
    TerminalSession out;
    host.SaveTabs(out);
    TR_ASSERT(t, out.tabs.size() == 2);
    TR_ASSERT(t, out.tabs[1].home == PanelView::HOME_RIGHT);
    TR_ASSERT(t, out.activeTabIdRight == std::optional<std::string>("2"));
    // Not auto: a resize changes nothing
    TR_ASSERT(t, host.SetLayoutSetting("portrait"));
    host.OnScreenResized(gedit::Rect(400, 50), true);
    TR_ASSERT(t, host.Right() == nullptr);
    return kTR_Pass;
}

// The toolkit side: a view added to another parent leaves the one it had; a split given a new child lets the old go
DLL_EXPORT int test_panellayout_reparent(ITesting *t) {
    RootView rootView;
    HSplitView split;
    rootView.AddView(&split);
    ViewBase a;
    ViewBase b;
    ViewBase lower;
    split.SetUpper(&a);
    split.SetLower(&lower);
    rootView.Initialize();
    TR_ASSERT(t, a.GetParentView() == &split);
    split.SetUpper(&b);
    TR_ASSERT(t, a.GetParentView() == nullptr);
    TR_ASSERT(t, b.GetParentView() == &split);
    VSplitView other;
    rootView.AddView(&other);
    other.SetLeft(&b);
    TR_ASSERT(t, b.GetParentView() == &other);
    // Twice to the same parent: once
    other.SetLeft(&b);
    TR_ASSERT(t, b.GetParentView() == &other);
    return kTR_Pass;
}

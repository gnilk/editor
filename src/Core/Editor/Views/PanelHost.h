//
// The panels in the view tree (hitlist #77, docs/usability-round.md §19): the bottom panel always, the right one while
// the layout is landscape and it has tabs. Owns both PanelViews and the split that puts the right one beside the
// workspace and editor; the rest of the tree is main.cpp's.
//
// The layout can change while running - '.layout portrait | landscape | auto', or 'auto' following the window - and a
// tab can move between the panels ('.panel move'). Nothing is closed for it: the tab views are handed from one panel
// to the other (PanelView::TakeTab / PutTab), so a running shell or agent goes on.
//

#ifndef GEDIT_PANELHOST_H
#define GEDIT_PANELHOST_H

#include <optional>
#include <string>

#include "Core/Config/ConfigNode.h"
#include "Core/Rect.h"
#include "Core/Session/SessionState.h"
#include "Core/UI/Views/HSplitView.h"
#include "Core/UI/Views/RootView.h"
#include "Core/UI/Views/VSplitView.h"
#include "PanelLayout.h"
#include "PanelView.h"

namespace gedit {
    class PanelHost {
    public:
        // The parts of the tree the panels go into: the split whose lower part is the bottom panel (the status line is
        // drawn on it), and what sits above it - the workspace and editor
        struct Parts {
            RootView *rootView = nullptr;
            HSplitView *statusSplit = nullptr;
            ViewBase *editorArea = nullptr;
        };
    public:
        PanelHost() = default;
        virtual ~PanelHost() = default;

        // The tabs from the session or config, placed by the layout setting ('main.layout') and the screen's size.
        // Before the tree is initialized.
        void Build(const Parts &parts, const TerminalSession &session, const ConfigNode &config, const std::string &layoutSetting, const Rect &dimensions);
        // The same, from a plan already made (the tests: tabs that start no shell)
        void BuildFromPlan(const Parts &parts, const PanelLayout::Plan &plan, PanelLayout::kLayout startLayout, const std::string &layoutSetting);
        // As top views, after the editor's (the first one registered gets the focus)
        void RegisterTopViews();

        // 'portrait' | 'landscape' | 'auto' - laid out now; false for anything else
        bool SetLayoutSetting(const std::string &setting);
        const std::string &LayoutSetting() const {
            return layoutSetting;
        }
        PanelLayout::kLayout Layout() const {
            return layout;
        }
        // auto: the window's new shape decides. From the root's resize, before the tree below it is laid out - so the
        // switch is not laid out here (the resize does it); else it is.
        void OnScreenResized(const Rect &dimensions, bool isLaidOutNext = false);

        PanelView &Bottom() {
            return bottom;
        }
        // nullptr while it isn't shown
        PanelView *Right() {
            return isRightShown ? &right : nullptr;
        }

        // Every tab, each with its home, and each panel's shown one
        void SaveTabs(TerminalSession &out);

        // The shown tab of the focused panel (a top view name) goes to the other one: its home changes, and in
        // landscape it moves there now. A line for the status bar.
        std::string MoveShownTab(const std::string &focusedTopView);

    public:
        static constexpr float RIGHT_PANEL_SPLIT = 0.6f;    // the workspace and editor get 3/5 of the width

    private:
        void Apply(PanelLayout::kLayout newLayout, bool isLaidOutNext = false);
        // Every tab homed 'home' from one panel to the other, in order
        void MoveTabsWithHome(PanelView &from, PanelView &to, const std::string &home, bool keepShown);
        void ShowRight(bool isShown);
        void Relayout();

    private:
        Parts parts = {};
        PanelView bottom;
        PanelView right;
        VSplitView panelsSplit;
        std::string layoutSetting = "portrait";
        PanelLayout::kLayout layout = PanelLayout::kLayout::kPortrait;
        bool isRightShown = false;
        bool areTopViewsRegistered = false;
        // The right panel's shown tab while it isn't shown (portrait) - for the next time it is
        std::optional<std::string> rightActiveId = {};
    };
}

#endif //GEDIT_PANELHOST_H

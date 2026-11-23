//
// Where the panels go (hitlist #77, docs/usability-round.md §19). Two layouts:
//   portrait  - today's: the editor on top, one panel below holding every tab
//   landscape - a full-height panel on the right for the tabs whose home is 'right' (panel_right.tabs - the agent),
//               the bottom one for the rest (panel.tabs - the build)
// 'main.layout: portrait | landscape | auto' - auto picks by the window's shape at start-up. A tab keeps its home
// whatever the layout: portrait shows the right-homed tabs in the bottom panel, and the session stores the home, so a
// start in the other layout puts each tab back. With no right-homed tab both layouts are today's.
//
// Plain data in, plain data out - the views are made and filled by main.cpp, saved through Editor.
//

#ifndef GEDIT_PANELLAYOUT_H
#define GEDIT_PANELLAYOUT_H

#include <optional>
#include <string>
#include <vector>

#include "Core/Config/ConfigNode.h"
#include "Core/Session/SessionState.h"
#include "PanelView.h"

namespace gedit {
    class PanelLayout {
    public:
        enum class kLayout {
            kPortrait,
            kLandscape,
        };
        // What goes in each panel, and the tab each shows. 'right' empty: no right panel.
        struct Plan {
            std::vector<PanelView::RestoredTab> bottom = {};
            std::vector<PanelView::RestoredTab> right = {};
            std::optional<std::string> activeBottom = {};
            std::optional<std::string> activeRight = {};
        };
    public:
        // 'portrait', 'landscape' or 'auto' (by the window's size in cells); anything else is portrait
        static kLayout Choose(const std::string &setting, int cols, int rows);
        // The session's tabs - else the configured ones (panel.tabs, then panel_right.tabs) - each in its home's panel
        // (landscape) or all in the bottom one (portrait). The bottom panel always has a terminal: one 'Shell' is added
        // when it would have none.
        static Plan PlanTabs(const TerminalSession &session, const ConfigNode &config, kLayout layout);
        // Every tab, the bottom panel's first, and each panel's shown one. 'right' nullptr (portrait): the right panel's
        // shown tab already in 'out' is kept for the next landscape start.
        static void SaveTabs(PanelView &bottom, PanelView *right, TerminalSession &out);

    public:
        // A window this much wider than tall (in pixels; a cell is taken as twice as tall as wide) is landscape -
        // a 16:10 or 16:9 screen is, a window on half of one is not
        static constexpr float LANDSCAPE_ASPECT = 1.5f;
        static constexpr float CELL_ASPECT = 2.0f;
    };
}

#endif //GEDIT_PANELLAYOUT_H

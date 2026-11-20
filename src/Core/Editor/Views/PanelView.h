//
// The lower panel (docs/panel-tabs.md, hitlist #53): the editor's part on top of the toolkit's TabbedView, which owns
// the tab views and draws the row. This part knows which kinds of tab there are (a terminal today; Messages next),
// builds them, makes a new terminal (Cmd+T / Ctrl+Shift+T in a terminal), closes the shown one (Cmd+W /
// Ctrl+Shift+W - asks when a job runs, keeps the last), and selects tabs with the keys that select files in the
// editor (SwitchToBuffer, CycleActiveBuffer*) while it has focus and more than one tab.
//
// A tab is a view kind and a config section (§7): 'panel.tabs: [ { name, view, config } ]' - the section, resolved
// (its 'inherit:' folded in), is handed to the tab when it is made. An agent's tab is a terminal with its own section
// ('ai_agent: { inherit: terminal, bootstrap: [claude] }').
//
// A restart brings back what was open (§6): the tabs, their names, kinds and sections and the shown one are in the
// session, each tab's scrollback and history in files named by its id ("" - the first tab - keeps the names from
// before tabs). 'panel.tabs' is used when the session has no tabs.
//

#ifndef GEDIT_PANELVIEW_H
#define GEDIT_PANELVIEW_H

#include <memory>
#include <string>
#include <vector>

#include "Core/Config/ConfigNode.h"
#include "Core/Session/SessionState.h"
#include "Core/UI/Views/TabbedView.h"
#include "TerminalView.h"

namespace gedit {
    class PanelView : public TabbedView {
    public:
        // What a tab is: its files' id, its name, what draws it and the config section it is handed
        struct TabSpec {
            std::string id = {};
            std::string name = {};
            std::string view = VIEW_TERMINAL;
            std::string config = VIEW_TERMINAL;
        };
    public:
        PanelView() = default;
        virtual ~PanelView() = default;

        // The tabs a session had - else the configured ones (panel.tabs), else one 'Shell'. Before the view tree is
        // initialized.
        void RestoreTabs(const TerminalSession &session);
        // Each tab's name, kind, section, files and scrollback, and the shown one
        void SaveTabs(TerminalSession &out);

        // The shown tab when it is a terminal, else the first terminal
        TerminalView *ActiveTerminal();
        // Where Cmd/Ctrl+Enter in the editor sends: the first terminal whose settings say 'promote_target: yes' (an
        // agent's tab, §7) - shown, so the text is seen arriving - else the active terminal
        TerminalView *PromoteTarget();
        // Every terminal, in tab order
        std::vector<TerminalView *> Terminals() const;
        // A new shell in a tab of its own, shown (and focused, when the panel has focus)
        void NewTerminal();
        // Asks first when a job runs in it; the last terminal stays
        void CloseTerminal(size_t idxTab);

        bool OnAction(const EditorAction &action) override;

        // 'panel.tabs' of a config: each tab's name, view (default 'terminal') and config (default the view's name).
        // A view kind this panel doesn't have is left out (and logged). Ids: "" for the first, then '2', '3'...
        static std::vector<TabSpec> ConfiguredTabs(const ConfigNode &config);
        // 'Shell', 'Shell 2', ... - the first number no tab has
        static std::string NextId(const std::vector<std::string> &ids);
        static std::string NameFor(const std::string &id);
        // What a tab shows: its title (OSC 0 / 2) when the shell or an app set one, cut to MAX_LABEL - else its name
        static std::u32string LabelFor(const std::string &name, const std::string &title);

    public:
        static constexpr size_t MAX_LABEL = 24;
        inline static const std::string VIEW_TERMINAL = "terminal";

    private:
        struct PanelTab {
            TerminalView *view = nullptr;   // owned by the TabbedView
            TabSpec spec;
        };
        TerminalView *AddTerminal(const TabSpec &spec, const std::string &scrollbackFile);
        void RemoveTerminal(size_t idxTab);
        PanelTab *PanelTabAt(size_t idxTab);
        // A tab number as the editor's files have it: 1..8, 9 the last
        void SelectTabNumber(int tabNumber);

    private:
        std::vector<PanelTab> panelTabs;
    };
}

#endif //GEDIT_PANELVIEW_H

//
// The lower panel (docs/panel-tabs.md, hitlist #53): the editor's part on top of the toolkit's TabbedView, which owns
// the tab views and draws the row. This part knows which kinds of tab there are - 'terminal' and 'messages' (the last
// build's errors and warnings, step 5) - builds them, makes a new terminal (Cmd+T / Ctrl+Shift+T in a tab), closes
// the shown tab (Cmd+W / Ctrl+Shift+W - asks when a job runs, keeps the last terminal), and selects tabs with the keys
// that select files in the editor (SwitchToBuffer, CycleActiveBuffer*) while it has focus and more than one tab.
//
// A tab is a view kind and a config section (§7): 'panel.tabs: [ { name, view, config } ]' - the section, resolved
// (its 'inherit:' folded in), is handed to the tab when it is made. An agent's tab is a terminal with its own section
// ('ai_agent: { inherit: terminal, bootstrap: [claude] }').
//
// A restart brings back what was open (§6): the tabs, their names, kinds and sections and the shown one are in the
// session, each tab's scrollback and history in files named by its id ("" - the first tab - keeps the names from
// before tabs). 'panel.tabs' is used when the session has no tabs.
//
// There can be two (hitlist #77, usability-round.md §19): the bottom one and, in the landscape layout, one on the right.
// Each tab has a home - the panel it belongs in - which PanelLayout uses to put it in one or the other; ids are unique
// over both (they name the tab's files), so a panel asks for the ids in use through its id source.
//
// The Editor Agent (hitlist #78, agent-integration.md §6.2) is a terminal tab the editor makes itself - never from
// 'panel.tabs' or the session: its name is fixed (the agent's title doesn't replace it), its label is marked, it is not
// saved, it doesn't move to the other panel, and it is never where dot-command output goes. Closing it stops the agent.
//

#ifndef GEDIT_PANELVIEW_H
#define GEDIT_PANELVIEW_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Core/Config/ConfigNode.h"
#include "Core/Session/SessionState.h"
#include "Core/UI/Views/TabbedView.h"
#include "MessagesView.h"
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
            std::string home = HOME_BOTTOM;
        };
        // A tab to make, and its terminal's saved scrollback (if any)
        struct RestoredTab {
            TabSpec spec;
            std::string scrollbackFile = {};
        };
        // The ids in use in every panel - a new tab's id is the first free one
        using IdSource = std::function<std::vector<std::string>()>;
        // A tab on its way to the other panel (a layout switch, '.panel move') - the view lives on, a shell keeps running
        struct MovedTab {
            std::unique_ptr<ViewBase> view = {};
            TabSpec spec = {};
            std::u32string infoLine = {};
            bool wasShown = false;
        };
    public:
        PanelView() = default;
        virtual ~PanelView() = default;

        // The tabs to make (PanelLayout::PlanTabs) and the one shown. Before the view tree is initialized.
        void AddTabs(const std::vector<RestoredTab> &tabs, const std::optional<std::string> &activeId);
        // Each tab's name, kind, section, home, files and scrollback - appended
        void CollectTabs(std::vector<TerminalTabSession> &out);
        std::optional<std::string> ActiveTabId();
        // Shows the tab with this id; false when there is none
        bool ShowTabId(const std::string &id);
        std::vector<std::string> Ids() const;
        // Every panel's ids, in one list - an id source over several panels
        static std::vector<std::string> IdsOf(const std::vector<const PanelView *> &panels);

        // Unset: this panel's own ids
        void SetIdSource(IdSource source) {
            idSource = std::move(source);
        }
        // The home a new tab gets when no tab is shown to take it from
        void SetDefaultHome(const std::string &home) {
            defaultHome = home;
        }
        // The name it is registered under as a top view - focus remembers it (Option/Alt+T)
        void SetTopViewName(const std::string &name) {
            topViewName = name;
        }
        const std::string &TopViewName() const {
            return topViewName;
        }
        // The bottom panel keeps its last terminal (the output console); the right one its last tab
        void SetKeepsLastTerminal(bool keeps) {
            keepsLastTerminal = keeps;
        }

        // The shown tab when it is a terminal, else the first terminal - never the Editor Agent's
        TerminalView *ActiveTerminal();
        // Where Cmd/Ctrl+Enter in the editor sends: the first terminal whose settings say 'promote_target: yes' (an
        // agent's tab, §7) - shown, so the text is seen arriving - else the active terminal
        TerminalView *PromoteTarget();
        // The first terminal whose settings say 'promote_target: yes', shown - nullptr when none says so
        TerminalView *FindPromoteTarget();
        // Every terminal, in tab order - the Editor Agent's too
        std::vector<TerminalView *> Terminals() const;

        // The Editor Agent's tab, made by the editor: a terminal with these settings (its bootstrap starts the agent),
        // the banner on a line of its own above it (the agent goes full screen - it would clear anything in the
        // terminal), shown - and focused when the panel has focus
        TerminalView *AddEditorAgent(const ConfigNode &settings, const std::string &banner, bool isShown = true);
        // nullptr when it isn't in this panel
        TerminalView *EditorAgent();
        bool IsEditorAgentTab(size_t idxTab);
        // A new shell in a tab of its own, shown (and focused, when the panel has focus) - homed as the shown tab is
        void NewTerminal();
        // ... homed here
        void NewTerminal(const std::string &home);

        // Out of this panel, view and spec - nothing is deleted; nullopt for no such tab
        std::optional<MovedTab> TakeTab(size_t idxTab);
        // Into this panel, last; shown when it was shown where it came from
        void PutTab(MovedTab tab);
        // The tabs whose home is 'home', in order
        std::vector<size_t> TabsWithHome(const std::string &home);
        std::string TabHome(size_t idxTab);
        void SetTabHome(size_t idxTab, const std::string &home);
        size_t NumTerminalsWithHome(const std::string &home);
        // A terminal asks first when a job runs in it; the last terminal stays
        void CloseTab(size_t idxTab);
        // Shows the first Messages tab; false when there is none (Cmd/Ctrl+F8 shows the list as a modal then)
        bool ShowMessages();

        bool OnAction(const EditorAction &action) override;

        // 'panel.tabs' of a config: each tab's name, view (default 'terminal') and config (default the view's name).
        // A view kind this panel doesn't have is left out (and logged). Ids: "" for the first, then '2', '3'...
        // A Messages tab's name defaults to 'Messages'.
        static std::vector<TabSpec> ConfiguredTabs(const ConfigNode &config);
        // '<section>.tabs' (panel, panel_right), each with that home; ids follow the ones in 'ids', which is added to
        static std::vector<TabSpec> ConfiguredTabs(const ConfigNode &config, const std::string &section, const std::string &home, std::vector<std::string> &ids);
        // 'Shell', 'Shell 2', ... - the first number no tab has
        static std::string NextId(const std::vector<std::string> &ids);
        static std::string NameFor(const std::string &id);
        // What a tab shows: its title (OSC 0 / 2) when the shell or an app set one, cut to MAX_LABEL - else its name
        static std::u32string LabelFor(const std::string &name, const std::string &title);

    public:
        static constexpr size_t MAX_LABEL = 24;
        inline static const std::string VIEW_TERMINAL = "terminal";
        inline static const std::string VIEW_MESSAGES = "messages";
        inline static const std::string HOME_BOTTOM = "bottom";
        inline static const std::string HOME_RIGHT = "right";
        // The Editor Agent's tab: its kind (never one 'panel.tabs' or a session can name), id, name and label mark
        inline static const std::string VIEW_EDITOR_AGENT = "editor_agent";
        inline static const std::string EDITOR_AGENT_ID = "editor_agent";
        inline static const std::string EDITOR_AGENT_NAME = "Editor Agent";
        inline static const std::string EDITOR_AGENT_COLOR = "tab_editor_agent";

    protected:
        // The labels that count first (a Messages tab's 'Messages (3)'), then the row
        void DrawViewContents() override;
        // Focus remembers the panel (Option/Alt+T goes back to it)
        void OnActivate(bool isNowActive) override;

    private:
        struct PanelTab {
            ViewBase *view = nullptr;   // owned by the TabbedView
            TabSpec spec;
            TerminalView *Terminal() const {
                return dynamic_cast<TerminalView *>(view);
            }
            bool IsEditorAgent() const {
                return spec.view == VIEW_EDITOR_AGENT;
            }
        };
        // By the spec's view kind; nullptr for a kind there isn't
        ViewBase *AddPanelTab(const TabSpec &spec, const std::string &scrollbackFile);
        TerminalView *AddTerminal(const TabSpec &spec, const std::string &scrollbackFile);
        // The label follows the terminal's title - in whichever panel it is
        void WatchTitle(TerminalView *view);
        MessagesView *AddMessages(const TabSpec &spec);
        void RemovePanelTab(size_t idxTab);
        // Its label: the name (not the agent's title), marked
        void MarkEditorAgent(TerminalView *view);
        // The Editor Agent's left out - it is no shell of the user's
        size_t NumTerminals();
        PanelTab *PanelTabAt(size_t idxTab);
        // A tab number as the editor's files have it: 1..8, 9 the last
        void SelectTabNumber(int tabNumber);
        std::vector<std::string> IdsInUse() const;
        // The shown tab's home - a new tab opened from it belongs where it does
        std::string HomeForNewTab();

    private:
        std::vector<PanelTab> panelTabs;
        IdSource idSource = {};
        std::string defaultHome = HOME_BOTTOM;
        std::string topViewName = {};
        bool keepsLastTerminal = true;
    };
}

#endif //GEDIT_PANELVIEW_H

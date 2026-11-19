//
// The lower panel: terminals in tabs (docs/panel-tabs.md, hitlist #53). The toolkit's TabbedView with the editor's
// part on top - it owns its terminals, makes a new one (Cmd+T / Ctrl+Shift+T in a terminal), closes the shown one
// (Cmd+W / Ctrl+Shift+W - asks when a job runs, keeps the last), and selects tabs with the keys that select files in
// the editor (SwitchToBuffer, CycleActiveBuffer*) while it has focus and more than one tab.
//
// A restart brings back what was open (§6): the tabs, their names and the shown one are in the session, each tab's
// scrollback and history in files named by its id ("" - the first terminal - keeps the names from before tabs).
//

#ifndef GEDIT_TERMINALPANELVIEW_H
#define GEDIT_TERMINALPANELVIEW_H

#include <memory>
#include <string>
#include <vector>

#include "Core/Session/SessionState.h"
#include "Core/UI/Views/TabbedView.h"
#include "TerminalView.h"

namespace gedit {
    class TerminalPanelView : public TabbedView {
    public:
        TerminalPanelView() = default;
        virtual ~TerminalPanelView() = default;

        // The tabs a session had - one 'Shell' when it had none. Before the view tree is initialized.
        void RestoreTabs(const TerminalSession &session);
        // Each tab's name, files and scrollback, and the shown one
        void SaveTabs(TerminalSession &out);

        // The shown tab when it is a terminal, else the first terminal
        TerminalView *ActiveTerminal();
        // A new shell in a tab of its own, shown (and focused, when the panel has focus)
        void NewTerminal();
        // Asks first when a job runs in it; the last terminal stays
        void CloseTerminal(size_t idxTab);

        bool OnAction(const EditorAction &action) override;

        // 'Shell', 'Shell 2', ... - the first number no tab has
        static std::string NextId(const std::vector<std::string> &ids);
        static std::string NameFor(const std::string &id);
        // What a tab shows: its title (OSC 0 / 2) when the shell or an app set one, cut to MAX_LABEL - else its name
        static std::u32string LabelFor(const std::string &name, const std::string &title);

    public:
        static constexpr size_t MAX_LABEL = 24;

    private:
        struct Terminal {
            std::unique_ptr<TerminalView> view;
            std::string id;
            std::string name;
        };
        TerminalView *AddTerminal(const std::string &id, const std::string &name, const std::string &scrollbackFile);
        void RemoveTerminal(size_t idxTab);
        Terminal *TerminalAt(size_t idxTab);
        // A tab number as the editor's files have it: 1..8, 9 the last
        void SelectTabNumber(int tabNumber);

    private:
        std::vector<Terminal> terminals;
    };
}

#endif //GEDIT_TERMINALPANELVIEW_H

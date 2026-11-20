//
// The Messages tab of the lower panel (docs/panel-tabs.md step 5, hitlist #52 / #53): the last build's errors and
// warnings, kept open - the Cmd/Ctrl+F8 list as a view, each message with the code it points at on the line under it
// (read once per run - a copy). Errors first, then warnings, as F8 steps them; it fills while
// the build runs (read when drawn - a terminal's output asks for the frames). The selected row follows F8 / Shift+F8;
// Enter or a double-click goes to the place (a jump - Cmd/Ctrl+[ comes back) and F8 goes on from there.
//
// It is handed the BuildMessages to show and its config section, resolved (panel.tabs: '{ view: messages }').
//

#ifndef GEDIT_MESSAGESVIEW_H
#define GEDIT_MESSAGESVIEW_H

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Core/Build/BuildMessages.h"
#include "Core/Config/ConfigNode.h"
#include "Core/Line.h"
#include "Core/UI/Views/ViewBase.h"

namespace gedit {
    class MessagesView : public ViewBase {
    public:
        explicit MessagesView(BuildMessages &newBuildMessages) : buildMessages(newBuildMessages) {
        }
        virtual ~MessagesView() = default;

        // Its config section, resolved ('keymap' - messages_keymap: default_keymap's arrows, Page Up / Down, Enter)
        void SetSettings(const ConfigNode &newSettings) {
            settings = newSettings;
        }

        // The tab's label: 'Messages (3)' - the errors and warnings listed; the name alone when there are none
        std::u32string Label(const std::string &name);
        // What the list shows now (read from BuildMessages), and where its selection is - rows from 0
        size_t NumRows();
        std::optional<size_t> SelectedRow();
        // The first line: the command and its counts, or how to get a list
        std::string Caption();

        void InitView() override;
        void ReInitView() override;
        void DrawViewContents() override;
        bool OnAction(const EditorAction &action) override;
        bool OnMouseEvent(const MouseEvent &mouseEvent) override;

        const std::u32string &GetStatusBarAbbreviation() override {
            static std::u32string abbreviation = U"MSG";
            return abbreviation;
        }

    public:
        static constexpr int LIST_TOP_ROW = 1;
        // A message, then its code
        static constexpr int LINES_PER_ROW = 2;

    protected:
        void OnActivate(bool isActivated) override;

    private:
        // Takes what BuildMessages has now; a new run starts at the top, a step (F8) moves the selection to it
        void Refresh();
        void MoveSelection(int delta);
        void SelectRow(size_t idxRow);
        void GoToSelected();
        // Rows that fit
        int ListHeight() const;
        // The code a message points at, read the first time it is drawn; nullptr when there is none
        Line::Ref CodeLine(size_t idxMessage, const ColorRGBA &background);

    private:
        BuildMessages &buildMessages;
        ConfigNode settings = ConfigNode(YAML::Node(YAML::NodeType::Map));
        std::vector<BuildMessage> messages;
        std::vector<size_t> rows;           // index into messages, per row (BuildMessages::Order)
        std::string caption;
        size_t idxSelected = 0;
        int idxTopRow = 0;
        uint64_t seenRun = 0;
        std::optional<size_t> seenCurrent;
        std::map<size_t, Line::Ref> codeLines;     // by index into messages, this run's
    };
}

#endif //GEDIT_MESSAGESVIEW_H

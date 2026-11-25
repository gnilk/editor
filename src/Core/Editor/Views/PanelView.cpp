//
// See PanelView.h
//

#include <algorithm>

#include "Core/Config/Config.h"
#include "Core/Editor.h"
#include "Core/Runloop.h"
#include "Core/RuntimeConfig.h"
#include "Core/UI/Modals/ConfirmModal.h"
#include "Core/UI/Views/RootView.h"
#include "Core/UnicodeHelper.h"
#include "PanelView.h"

using namespace gedit;

void PanelView::AddTabs(const std::vector<RestoredTab> &tabs, const std::optional<std::string> &activeId) {
    for (auto &tab : tabs) {
        AddPanelTab(tab.spec, tab.scrollbackFile);
    }
    if (activeId.has_value()) {
        ShowTabId(activeId.value());
    }
}

bool PanelView::ShowTabId(const std::string &id) {
    for (size_t i = 0; i < NumTabs(); i++) {
        auto panelTab = PanelTabAt(i);
        if ((panelTab != nullptr) && (panelTab->spec.id == id)) {
            SetActiveTab(i);
            return true;
        }
    }
    return false;
}

void PanelView::CollectTabs(std::vector<TerminalTabSession> &out) {
    for (size_t i = 0; i < NumTabs(); i++) {
        auto panelTab = PanelTabAt(i);
        // The Editor Agent isn't brought back - the editor starts it again when it is asked to
        if ((panelTab == nullptr) || panelTab->IsEditorAgent()) {
            continue;
        }
        TerminalTabSession tab;
        tab.id = panelTab->spec.id;
        tab.name = panelTab->spec.name;
        tab.view = panelTab->spec.view;
        tab.config = panelTab->spec.config;
        tab.home = panelTab->spec.home;
        if (auto terminal = panelTab->Terminal()) {
            terminal->GetController().ToSession(tab.scrollbackFile);
        }
        out.push_back(tab);
    }
}

std::vector<std::string> PanelView::IdsOf(const std::vector<const PanelView *> &panels) {
    // Sized first, then filled - not grown with insert (trun's ASan reads that as a container overflow, CLAUDE.md)
    size_t numIds = 0;
    for (auto panel : panels) {
        numIds += panel->panelTabs.size();
    }
    std::vector<std::string> ids;
    ids.reserve(numIds);
    for (auto panel : panels) {
        for (auto &panelTab : panel->panelTabs) {
            ids.push_back(panelTab.spec.id);
        }
    }
    return ids;
}

std::optional<std::string> PanelView::ActiveTabId() {
    auto idxTab = ActiveTab();
    auto panelTab = idxTab.has_value() ? PanelTabAt(idxTab.value()) : nullptr;
    if (panelTab == nullptr) {
        return {};
    }
    return panelTab->spec.id;
}

std::vector<std::string> PanelView::Ids() const {
    std::vector<std::string> ids;
    for (auto &panelTab : panelTabs) {
        ids.push_back(panelTab.spec.id);
    }
    return ids;
}

TerminalView *PanelView::ActiveTerminal() {
    auto idxShown = ActiveTab();
    if (auto shown = dynamic_cast<TerminalView *>(ActiveView()); (shown != nullptr) && !IsEditorAgentTab(idxShown.value())) {
        return shown;
    }
    for (size_t i = 0; i < NumTabs(); i++) {
        auto terminal = dynamic_cast<TerminalView *>(TabView(i));
        if ((terminal != nullptr) && !IsEditorAgentTab(i)) {
            return terminal;
        }
    }
    return nullptr;
}

TerminalView *PanelView::PromoteTarget() {
    if (auto target = FindPromoteTarget()) {
        return target;
    }
    return ActiveTerminal();
}

TerminalView *PanelView::FindPromoteTarget() {
    for (auto terminal : Terminals()) {
        if (terminal->GetController().Settings().GetBool("promote_target", false)) {
            if (auto idxTab = IndexOf(terminal)) {
                SetActiveTab(idxTab.value());
            }
            return terminal;
        }
    }
    return nullptr;
}

std::vector<TerminalView *> PanelView::Terminals() const {
    std::vector<TerminalView *> terminals;
    for (size_t i = 0; i < NumTabs(); i++) {
        if (auto terminal = dynamic_cast<TerminalView *>(TabView(i))) {
            terminals.push_back(terminal);
        }
    }
    return terminals;
}

TerminalView *PanelView::AddEditorAgent(const ConfigNode &settings, const std::string &banner, bool isShown) {
    TabSpec spec;
    spec.id = EDITOR_AGENT_ID;
    spec.name = EDITOR_AGENT_NAME;
    spec.view = VIEW_EDITOR_AGENT;
    spec.config = VIEW_TERMINAL;
    spec.home = HOME_BOTTOM;
    auto newView = std::make_unique<TerminalView>();
    auto view = newView.get();
    view->SetSettings(settings);
    auto &controller = view->GetController();
    controller.SetSessionTab(spec.id, "");
    panelTabs.push_back({view, spec});
    // Starts its shell, which runs the bootstrap - the agent (TerminalView::InitView)
    AddTab(std::move(newView), LabelFor(spec.name, ""));
    MarkEditorAgent(view);
    if (auto idxTab = IndexOf(view)) {
        SetTabInfoLine(idxTab.value(), UnicodeHelper::utf8to32(banner));
    }
    if (auto idxTab = IndexOf(view); idxTab.has_value() && isShown) {
        SetActiveTab(idxTab.value());
    }
    return view;
}

TerminalView *PanelView::EditorAgent() {
    for (auto &panelTab : panelTabs) {
        if (panelTab.IsEditorAgent()) {
            return panelTab.Terminal();
        }
    }
    return nullptr;
}

bool PanelView::RemoveEditorAgent() {
    auto idxTab = IndexOf(EditorAgent());
    if (!idxTab.has_value()) {
        return false;
    }
    bool wasShown = (ActiveTab() == idxTab);
    RemovePanelTab(idxTab.value());
    return wasShown;
}

bool PanelView::IsEditorAgentTab(size_t idxTab) {
    auto panelTab = PanelTabAt(idxTab);
    return (panelTab != nullptr) && panelTab->IsEditorAgent();
}

void PanelView::NewTerminal() {
    NewTerminal(HomeForNewTab());
}

void PanelView::NewTerminal(const std::string &home) {
    auto id = NextId(IdsInUse());
    TabSpec spec;
    spec.id = id;
    spec.name = NameFor(id);
    spec.home = home;
    auto view = AddTerminal(spec, "");
    if (auto idxTab = IndexOf(view)) {
        SetActiveTab(idxTab.value());
    }
}

void PanelView::CloseTab(size_t idxTab) {
    auto panelTab = PanelTabAt(idxTab);
    if (panelTab == nullptr) {
        return;
    }
    auto terminal = panelTab->Terminal();
    // The Editor Agent's tab goes whatever else is left; closing it stops the agent - said before it happens
    if (panelTab->IsEditorAgent() && (terminal != nullptr)) {
        if (!terminal->GetController().HasRunningJob() || !RuntimeConfig::Instance().HasRootView()) {
            RemovePanelTab(idxTab);
            return;
        }
        auto view = panelTab->view;
        auto modal = ConfirmModal::Create("Close the Editor Agent", U"The Editor Agent is running - closing its tab stops it.", {U"Stop it", U"Keep it"});
        modal->SetOnChoice([this, view](int idxChoice) {
            auto idxNow = IndexOf(view);
            if ((idxChoice == 0) && idxNow.has_value()) {
                RemovePanelTab(idxNow.value());
            }
        });
        RuntimeConfig::Instance().GetRootViewAs<RootView>()->ShowModal(std::move(modal));
        return;
    }
    if ((terminal == nullptr) && (keepsLastTerminal || (NumTabs() > 1))) {
        RemovePanelTab(idxTab);
        return;
    }
    if (terminal == nullptr) {
        Editor::Instance().ShowStatusMessage(U"The last tab stays");
        return;
    }
    if (keepsLastTerminal && (NumTerminals() == 1)) {
        Editor::Instance().ShowStatusMessage(U"The last terminal stays");
        return;
    }
    if (!keepsLastTerminal && (NumTabs() == 1)) {
        Editor::Instance().ShowStatusMessage(U"The last tab stays");
        return;
    }
    if (!terminal->GetController().HasRunningJob() || !RuntimeConfig::Instance().HasRootView()) {
        RemovePanelTab(idxTab);
        return;
    }
    auto view = panelTab->view;
    auto message = UnicodeHelper::utf8to32("A job is running in '" + panelTab->spec.name + "' - close it anyway?");
    auto modal = ConfirmModal::Create("Close terminal", message, {U"Close", U"Keep it"});
    modal->SetOnChoice([this, view](int idxChoice) {
        // The tab may have moved while the question was up
        auto idxNow = IndexOf(view);
        if ((idxChoice == 0) && idxNow.has_value()) {
            RemovePanelTab(idxNow.value());
        }
    });
    RuntimeConfig::Instance().GetRootViewAs<RootView>()->ShowModal(std::move(modal));
}

bool PanelView::ShowMessages() {
    for (size_t i = 0; i < NumTabs(); i++) {
        if (dynamic_cast<MessagesView *>(TabView(i)) != nullptr) {
            SetActiveTab(i);
            return true;
        }
    }
    return false;
}

void PanelView::DrawViewContents() {
    for (size_t i = 0; i < NumTabs(); i++) {
        auto panelTab = PanelTabAt(i);
        if (auto messages = (panelTab != nullptr) ? dynamic_cast<MessagesView *>(panelTab->view) : nullptr) {
            SetTabLabel(i, messages->Label(panelTab->spec.name));
        }
    }
    TabbedView::DrawViewContents();
}

void PanelView::OnActivate(bool isNowActive) {
    TabbedView::OnActivate(isNowActive);
    if (isNowActive && !topViewName.empty()) {
        Editor::Instance().NotePanelFocused(topViewName);
    }
}

bool PanelView::OnAction(const EditorAction &action) {
    // Ours before the terminal's - an app in it would otherwise get the key
    if (action.action == kAction::kActionNewTerminal) {
        NewTerminal();
        return true;
    }
    if (action.action == kAction::kActionCloseTerminal) {
        if (auto idxTab = ActiveTab()) {
            CloseTab(idxTab.value());
        }
        return true;
    }
    // The keys that select files select tabs while there are tabs to select (§6)
    if (NumTabs() > 1) {
        if (action.action == kAction::kActionCycleActiveBufferNext) {
            NextTab();
            return true;
        }
        if (action.action == kAction::kActionCycleActiveBufferPrev) {
            PreviousTab();
            return true;
        }
        if (action.action == kAction::kActionSwitchToBuffer) {
            auto key = action.keyPress.key;
            if ((key >= '1') && (key <= '9')) {
                SelectTabNumber(key - '0');
            }
            return true;
        }
    }
    return TabbedView::OnAction(action);
}

std::vector<PanelView::TabSpec> PanelView::ConfiguredTabs(const ConfigNode &config) {
    std::vector<std::string> ids;
    return ConfiguredTabs(config, "panel", HOME_BOTTOM, ids);
}

std::vector<PanelView::TabSpec> PanelView::ConfiguredTabs(const ConfigNode &config, const std::string &section, const std::string &home, std::vector<std::string> &ids) {
    std::vector<TabSpec> specs;
    auto tabs = ConfigNode::NodeAtPath(config.GetDataNode(), {section, "tabs"});
    if (!tabs.IsSequence()) {
        return specs;
    }
    for (auto const &tabNode : tabs) {
        if (!tabNode.IsMap()) {
            continue;
        }
        ConfigNode tab(tabNode);
        TabSpec spec;
        spec.view = tab.GetStr("view", VIEW_TERMINAL);
        if ((spec.view != VIEW_TERMINAL) && (spec.view != VIEW_MESSAGES)) {
            gnilk::Logger::GetLogger("PanelView")->Warn("%s.tabs: no view '%s' - tab left out", section.c_str(), spec.view.c_str());
            continue;
        }
        spec.config = tab.GetStr("config", spec.view);
        spec.id = ids.empty() ? "" : NextId(ids);
        spec.name = tab.GetStr("name", (spec.view == VIEW_MESSAGES) ? std::string("Messages") : NameFor(spec.id));
        spec.home = home;
        ids.push_back(spec.id);
        specs.push_back(spec);
    }
    return specs;
}

std::string PanelView::NextId(const std::vector<std::string> &ids) {
    for (int n = 2;; n++) {
        auto id = std::to_string(n);
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
            return id;
        }
    }
}

std::string PanelView::NameFor(const std::string &id) {
    return id.empty() ? std::string("Shell") : "Shell " + id;
}

std::u32string PanelView::LabelFor(const std::string &name, const std::string &title) {
    auto label = UnicodeHelper::utf8to32(title.empty() ? name : title);
    if (label.size() > MAX_LABEL) {
        label = label.substr(0, MAX_LABEL - 1) + U"…";
    }
    return label;
}

ViewBase *PanelView::AddPanelTab(const TabSpec &spec, const std::string &scrollbackFile) {
    if (spec.view == VIEW_TERMINAL) {
        return AddTerminal(spec, scrollbackFile);
    }
    if (spec.view == VIEW_MESSAGES) {
        return AddMessages(spec);
    }
    gnilk::Logger::GetLogger("PanelView")->Warn("Tab '%s': no view '%s' - left out", spec.name.c_str(), spec.view.c_str());
    return nullptr;
}

TerminalView *PanelView::AddTerminal(const TabSpec &spec, const std::string &scrollbackFile) {
    auto newView = std::make_unique<TerminalView>();
    auto view = newView.get();
    // Its settings, resolved once here - the terminal never looks a section up (§7)
    auto settings = Config::Instance().ResolveSection(spec.config);
    if (!settings.GetDataNode().IsDefined()) {
        gnilk::Logger::GetLogger("PanelView")->Warn("Tab '%s': no config section '%s' - using 'terminal'", spec.name.c_str(), spec.config.c_str());
        settings = Config::Instance().ResolveSection(VIEW_TERMINAL);
    }
    view->SetSettings(settings);
    auto &controller = view->GetController();
    controller.SetSessionTab(spec.id, scrollbackFile);
    WatchTitle(view);
    panelTabs.push_back({view, spec});
    // Starts its shell (TerminalView::InitView)
    AddTab(std::move(newView), LabelFor(spec.name, ""));
    return view;
}

void PanelView::WatchTitle(TerminalView *view) {
    // The pty thread: the label changes on the UI thread, if the tab is still here
    view->GetController().SetTitleChangedHandler([this, view]() {
        Runloop::PostMessage(0x00, [this, view](uint32_t) {
            auto idxTab = IndexOf(view);
            auto shown = idxTab.has_value() ? PanelTabAt(idxTab.value()) : nullptr;
            if (shown != nullptr) {
                SetTabLabel(idxTab.value(), LabelFor(shown->spec.name, view->GetController().GetTitle()));
            }
        });
    });
}

std::optional<PanelView::MovedTab> PanelView::TakeTab(size_t idxTab) {
    auto view = TabView(idxTab);
    auto it = std::find_if(panelTabs.begin(), panelTabs.end(), [view](const PanelTab &panelTab) {
        return panelTab.view == view;
    });
    if ((view == nullptr) || (it == panelTabs.end())) {
        return {};
    }
    MovedTab moved;
    moved.spec = it->spec;
    moved.wasShown = (ActiveTab() == idxTab);
    moved.infoLine = TabInfoLine(idxTab);
    panelTabs.erase(it);
    moved.view = TabbedView::TakeTab(idxTab);
    return moved;
}

void PanelView::PutTab(MovedTab tab) {
    auto view = tab.view.get();
    if (view == nullptr) {
        return;
    }
    auto terminal = dynamic_cast<TerminalView *>(view);
    bool isEditorAgent = (tab.spec.view == VIEW_EDITOR_AGENT);
    auto title = ((terminal != nullptr) && !isEditorAgent) ? terminal->GetController().GetTitle() : std::string();
    panelTabs.push_back({view, tab.spec});
    if ((terminal != nullptr) && !isEditorAgent) {
        WatchTitle(terminal);
    }
    AddTab(std::move(tab.view), LabelFor(tab.spec.name, title));
    if (isEditorAgent && (terminal != nullptr)) {
        MarkEditorAgent(terminal);
    }
    if (auto idxTab = IndexOf(view); idxTab.has_value() && !tab.infoLine.empty()) {
        SetTabInfoLine(idxTab.value(), tab.infoLine);
    }
    if (tab.wasShown) {
        if (auto idxTab = IndexOf(view)) {
            SetActiveTab(idxTab.value());
        }
    }
}

std::vector<size_t> PanelView::TabsWithHome(const std::string &home) {
    std::vector<size_t> indices;
    for (size_t i = 0; i < NumTabs(); i++) {
        auto panelTab = PanelTabAt(i);
        if ((panelTab != nullptr) && (panelTab->spec.home == home)) {
            indices.push_back(i);
        }
    }
    return indices;
}

std::string PanelView::TabHome(size_t idxTab) {
    auto panelTab = PanelTabAt(idxTab);
    return (panelTab != nullptr) ? panelTab->spec.home : std::string();
}

void PanelView::SetTabHome(size_t idxTab, const std::string &home) {
    if (auto panelTab = PanelTabAt(idxTab)) {
        panelTab->spec.home = home;
    }
}

size_t PanelView::NumTerminalsWithHome(const std::string &home) {
    size_t num = 0;
    for (size_t i = 0; i < NumTabs(); i++) {
        auto panelTab = PanelTabAt(i);
        if ((panelTab != nullptr) && (panelTab->spec.home == home) && (panelTab->Terminal() != nullptr) && !panelTab->IsEditorAgent()) {
            num++;
        }
    }
    return num;
}

// The last build's messages - the editor's one list - and its section ('keymap'); a section isn't needed
MessagesView *PanelView::AddMessages(const TabSpec &spec) {
    auto newView = std::make_unique<MessagesView>(Editor::Instance().GetBuildMessages());
    auto view = newView.get();
    auto settings = Config::Instance().ResolveSection(spec.config);
    if (settings.GetDataNode().IsDefined()) {
        view->SetSettings(settings);
    }
    panelTabs.push_back({view, spec});
    AddTab(std::move(newView), LabelFor(spec.name, ""));
    return view;
}

void PanelView::RemovePanelTab(size_t idxTab) {
    auto view = TabView(idxTab);
    auto it = std::find_if(panelTabs.begin(), panelTabs.end(), [view](const PanelTab &panelTab) {
        return panelTab.view == view;
    });
    if (it == panelTabs.end()) {
        return;
    }
    if (auto terminal = it->Terminal()) {
        terminal->GetController().ForgetSession();
    }
    panelTabs.erase(it);
    // Deletes the view - a terminal's shell ends with its controller
    RemoveTab(idxTab);
    // Dot-command output and the JS console go to a terminal that is still there - the bottom panel's, when this one
    // has none left (the right panel can hold only a Messages tab)
    if (auto shown = ActiveTerminal()) {
        shown->GetController().RegisterAsOutputConsole();
    } else {
        Editor::Instance().RegisterPanelOutputConsole();
    }
    Editor::Instance().TriggerUIRedraw();
}

void PanelView::MarkEditorAgent(TerminalView *view) {
    // A violet of its own when the theme has no 'tab_editor_agent'
    static const auto fallback = ColorRGBA::FromRGB(0xc5, 0x86, 0xc0);
    if (auto idxTab = IndexOf(view)) {
        SetTabLabel(idxTab.value(), LabelFor(EDITOR_AGENT_NAME, ""));
        SetTabMark(idxTab.value(), EDITOR_AGENT_COLOR, fallback);
    }
}

size_t PanelView::NumTerminals() {
    size_t num = 0;
    for (size_t i = 0; i < NumTabs(); i++) {
        if ((dynamic_cast<TerminalView *>(TabView(i)) != nullptr) && !IsEditorAgentTab(i)) {
            num++;
        }
    }
    return num;
}

PanelView::PanelTab *PanelView::PanelTabAt(size_t idxTab) {
    auto view = TabView(idxTab);
    for (auto &panelTab : panelTabs) {
        if (panelTab.view == view) {
            return &panelTab;
        }
    }
    return nullptr;
}

void PanelView::SelectTabNumber(int tabNumber) {
    if ((tabNumber < 1) || (NumTabs() == 0)) {
        return;
    }
    auto idxTab = (tabNumber == 9) ? NumTabs() - 1 : (size_t)(tabNumber - 1);
    if (idxTab < NumTabs()) {
        SetActiveTab(idxTab);
    }
}

std::vector<std::string> PanelView::IdsInUse() const {
    return idSource ? idSource() : Ids();
}

std::string PanelView::HomeForNewTab() {
    auto idxTab = ActiveTab();
    auto panelTab = idxTab.has_value() ? PanelTabAt(idxTab.value()) : nullptr;
    return (panelTab != nullptr) ? panelTab->spec.home : defaultHome;
}

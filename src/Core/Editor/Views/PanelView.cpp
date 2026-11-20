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

void PanelView::RestoreTabs(const TerminalSession &session) {
    if (!session.tabs.empty()) {
        for (auto &tab : session.tabs) {
            TabSpec spec;
            spec.id = tab.id;
            spec.name = tab.name.empty() ? NameFor(tab.id) : tab.name;
            spec.view = tab.view.empty() ? VIEW_TERMINAL : tab.view;
            spec.config = tab.config.empty() ? spec.view : tab.config;
            AddTerminal(spec, tab.scrollbackFile);
        }
        if ((session.activeTab > 0) && ((size_t)session.activeTab < NumTabs())) {
            SetActiveTab((size_t)session.activeTab);
        }
        return;
    }
    // No tabs in the session (none yet, or one from before tabs - its one terminal's files go to the first tab)
    auto specs = ConfiguredTabs(Config::Instance());
    if (specs.empty()) {
        specs.push_back({"", NameFor("")});
    }
    for (auto &spec : specs) {
        AddTerminal(spec, spec.id.empty() ? session.scrollbackFile : "");
    }
}

void PanelView::SaveTabs(TerminalSession &out) {
    out = TerminalSession{};
    for (size_t i = 0; i < NumTabs(); i++) {
        auto panelTab = PanelTabAt(i);
        if (panelTab == nullptr) {
            continue;
        }
        TerminalTabSession tab;
        tab.id = panelTab->spec.id;
        tab.name = panelTab->spec.name;
        tab.view = panelTab->spec.view;
        tab.config = panelTab->spec.config;
        panelTab->view->GetController().ToSession(tab.scrollbackFile);
        if (tab.id.empty()) {
            out.scrollbackFile = tab.scrollbackFile;
        }
        out.tabs.push_back(tab);
    }
    out.activeTab = (int32_t)ActiveTab().value_or(0);
}

TerminalView *PanelView::ActiveTerminal() {
    if (auto shown = dynamic_cast<TerminalView *>(ActiveView())) {
        return shown;
    }
    return panelTabs.empty() ? nullptr : panelTabs.front().view;
}

TerminalView *PanelView::PromoteTarget() {
    for (auto &panelTab : panelTabs) {
        if (panelTab.view->GetController().Settings().GetBool("promote_target", false)) {
            if (auto idxTab = IndexOf(panelTab.view)) {
                SetActiveTab(idxTab.value());
            }
            return panelTab.view;
        }
    }
    return ActiveTerminal();
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

void PanelView::NewTerminal() {
    std::vector<std::string> ids;
    for (auto &panelTab : panelTabs) {
        ids.push_back(panelTab.spec.id);
    }
    auto id = NextId(ids);
    auto view = AddTerminal({id, NameFor(id)}, "");
    if (auto idxTab = IndexOf(view)) {
        SetActiveTab(idxTab.value());
    }
}

void PanelView::CloseTerminal(size_t idxTab) {
    auto panelTab = PanelTabAt(idxTab);
    if (panelTab == nullptr) {
        return;
    }
    if (panelTabs.size() == 1) {
        Editor::Instance().ShowStatusMessage(U"The last terminal stays");
        return;
    }
    if (!panelTab->view->GetController().HasRunningJob() || !RuntimeConfig::Instance().HasRootView()) {
        RemoveTerminal(idxTab);
        return;
    }
    auto view = panelTab->view;
    auto message = UnicodeHelper::utf8to32("A job is running in '" + panelTab->spec.name + "' - close it anyway?");
    auto modal = ConfirmModal::Create("Close terminal", message, {U"Close", U"Keep it"});
    modal->SetOnChoice([this, view](int idxChoice) {
        // The tab may have moved while the question was up
        auto idxNow = IndexOf(view);
        if ((idxChoice == 0) && idxNow.has_value()) {
            RemoveTerminal(idxNow.value());
        }
    });
    RuntimeConfig::Instance().GetRootViewAs<RootView>()->ShowModal(std::move(modal));
}

bool PanelView::OnAction(const EditorAction &action) {
    // Ours before the terminal's - an app in it would otherwise get the key
    if (action.action == kAction::kActionNewTerminal) {
        NewTerminal();
        return true;
    }
    if (action.action == kAction::kActionCloseTerminal) {
        if (auto idxTab = ActiveTab()) {
            CloseTerminal(idxTab.value());
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
    std::vector<TabSpec> specs;
    auto tabs = ConfigNode::NodeAtPath(config.GetDataNode(), {"panel", "tabs"});
    if (!tabs.IsSequence()) {
        return specs;
    }
    std::vector<std::string> ids;
    for (auto const &tabNode : tabs) {
        if (!tabNode.IsMap()) {
            continue;
        }
        ConfigNode tab(tabNode);
        TabSpec spec;
        spec.view = tab.GetStr("view", VIEW_TERMINAL);
        if (spec.view != VIEW_TERMINAL) {
            gnilk::Logger::GetLogger("PanelView")->Warn("panel.tabs: no view '%s' - tab left out", spec.view.c_str());
            continue;
        }
        spec.config = tab.GetStr("config", spec.view);
        spec.id = ids.empty() ? "" : NextId(ids);
        spec.name = tab.GetStr("name", NameFor(spec.id));
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
    // The pty thread: the label changes on the UI thread, if the tab is still there
    controller.SetTitleChangedHandler([this, view]() {
        Runloop::PostMessage(0x00, [this, view](uint32_t) {
            auto idxTab = IndexOf(view);
            auto shown = idxTab.has_value() ? PanelTabAt(idxTab.value()) : nullptr;
            if (shown != nullptr) {
                SetTabLabel(idxTab.value(), LabelFor(shown->spec.name, view->GetController().GetTitle()));
            }
        });
    });
    panelTabs.push_back({view, spec});
    // Starts its shell (TerminalView::InitView)
    AddTab(std::move(newView), LabelFor(spec.name, ""));
    return view;
}

void PanelView::RemoveTerminal(size_t idxTab) {
    auto view = TabView(idxTab);
    auto it = std::find_if(panelTabs.begin(), panelTabs.end(), [view](const PanelTab &panelTab) {
        return panelTab.view == view;
    });
    if (it == panelTabs.end()) {
        return;
    }
    it->view->GetController().ForgetSession();
    panelTabs.erase(it);
    // Deletes the view - its shell ends with the controller
    RemoveTab(idxTab);
    // Dot-command output and the JS console go to a terminal that is still there
    if (auto shown = ActiveTerminal()) {
        shown->GetController().RegisterAsOutputConsole();
    }
    Editor::Instance().TriggerUIRedraw();
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

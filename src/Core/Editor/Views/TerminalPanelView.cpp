//
// See TerminalPanelView.h
//

#include <algorithm>

#include "Core/Editor.h"
#include "Core/Runloop.h"
#include "Core/RuntimeConfig.h"
#include "Core/UI/Modals/ConfirmModal.h"
#include "Core/UI/Views/RootView.h"
#include "Core/UnicodeHelper.h"
#include "TerminalPanelView.h"

using namespace gedit;

void TerminalPanelView::RestoreTabs(const TerminalSession &session) {
    if (session.tabs.empty()) {
        // A session from before tabs, or none: the one terminal, with the files it always had
        AddTerminal("", NameFor(""), session.scrollbackFile);
        return;
    }
    for (auto &tab : session.tabs) {
        AddTerminal(tab.id, tab.name.empty() ? NameFor(tab.id) : tab.name, tab.scrollbackFile);
    }
    if ((session.activeTab > 0) && ((size_t)session.activeTab < NumTabs())) {
        SetActiveTab((size_t)session.activeTab);
    }
}

void TerminalPanelView::SaveTabs(TerminalSession &out) {
    out = TerminalSession{};
    for (size_t i = 0; i < NumTabs(); i++) {
        auto terminal = TerminalAt(i);
        if (terminal == nullptr) {
            continue;
        }
        TerminalTabSession tab;
        tab.id = terminal->id;
        tab.name = terminal->name;
        terminal->view->GetController().ToSession(tab.scrollbackFile);
        if (tab.id.empty()) {
            out.scrollbackFile = tab.scrollbackFile;
        }
        out.tabs.push_back(tab);
    }
    out.activeTab = (int32_t)ActiveTab().value_or(0);
}

TerminalView *TerminalPanelView::ActiveTerminal() {
    if (auto shown = dynamic_cast<TerminalView *>(ActiveView())) {
        return shown;
    }
    return terminals.empty() ? nullptr : terminals.front().view.get();
}

void TerminalPanelView::NewTerminal() {
    std::vector<std::string> ids;
    for (auto &terminal : terminals) {
        ids.push_back(terminal.id);
    }
    auto id = NextId(ids);
    auto view = AddTerminal(id, NameFor(id), "");
    if (auto idxTab = IndexOf(view)) {
        SetActiveTab(idxTab.value());
    }
}

void TerminalPanelView::CloseTerminal(size_t idxTab) {
    auto terminal = TerminalAt(idxTab);
    if (terminal == nullptr) {
        return;
    }
    if (terminals.size() == 1) {
        Editor::Instance().ShowStatusMessage(U"The last terminal stays");
        return;
    }
    if (!terminal->view->GetController().HasRunningJob() || !RuntimeConfig::Instance().HasRootView()) {
        RemoveTerminal(idxTab);
        return;
    }
    auto view = terminal->view.get();
    auto message = UnicodeHelper::utf8to32("A job is running in '" + terminal->name + "' - close it anyway?");
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

bool TerminalPanelView::OnAction(const EditorAction &action) {
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

std::string TerminalPanelView::NextId(const std::vector<std::string> &ids) {
    for (int n = 2;; n++) {
        auto id = std::to_string(n);
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
            return id;
        }
    }
}

std::string TerminalPanelView::NameFor(const std::string &id) {
    return id.empty() ? std::string("Shell") : "Shell " + id;
}

std::u32string TerminalPanelView::LabelFor(const std::string &name, const std::string &title) {
    auto label = UnicodeHelper::utf8to32(title.empty() ? name : title);
    if (label.size() > MAX_LABEL) {
        label = label.substr(0, MAX_LABEL - 1) + U"…";
    }
    return label;
}

TerminalView *TerminalPanelView::AddTerminal(const std::string &id, const std::string &name, const std::string &scrollbackFile) {
    Terminal terminal;
    terminal.view = std::make_unique<TerminalView>();
    terminal.id = id;
    terminal.name = name;
    auto view = terminal.view.get();
    auto &controller = view->GetController();
    controller.SetSessionTab(id, scrollbackFile);
    // The pty thread: the label changes on the UI thread, if the tab is still there
    controller.SetTitleChangedHandler([this, view]() {
        Runloop::PostMessage(0x00, [this, view](uint32_t) {
            auto idxTab = IndexOf(view);
            auto terminal = idxTab.has_value() ? TerminalAt(idxTab.value()) : nullptr;
            if (terminal != nullptr) {
                SetTabLabel(idxTab.value(), LabelFor(terminal->name, view->GetController().GetTitle()));
            }
        });
    });
    terminals.push_back(std::move(terminal));
    // Starts its shell (TerminalView::InitView)
    AddTab(view, LabelFor(name, ""));
    return view;
}

void TerminalPanelView::RemoveTerminal(size_t idxTab) {
    auto view = TabView(idxTab);
    auto it = std::find_if(terminals.begin(), terminals.end(), [view](const Terminal &terminal) {
        return terminal.view.get() == view;
    });
    if (it == terminals.end()) {
        return;
    }
    it->view->GetController().ForgetSession();
    RemoveTab(idxTab);
    // Its shell ends with the controller
    terminals.erase(it);
    // Dot-command output and the JS console go to a terminal that is still there
    if (auto shown = ActiveTerminal()) {
        shown->GetController().RegisterAsOutputConsole();
    }
    Editor::Instance().TriggerUIRedraw();
}

TerminalPanelView::Terminal *TerminalPanelView::TerminalAt(size_t idxTab) {
    auto view = TabView(idxTab);
    for (auto &terminal : terminals) {
        if (terminal.view.get() == view) {
            return &terminal;
        }
    }
    return nullptr;
}

void TerminalPanelView::SelectTabNumber(int tabNumber) {
    if ((tabNumber < 1) || (NumTabs() == 0)) {
        return;
    }
    auto idxTab = (tabNumber == 9) ? NumTabs() - 1 : (size_t)(tabNumber - 1);
    if (idxTab < NumTabs()) {
        SetActiveTab(idxTab);
    }
}

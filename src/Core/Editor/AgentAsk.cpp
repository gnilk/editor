//
// See AgentAsk.h
//

#include <ctime>
#include <fstream>

#include <fmt/format.h>

#include "AgentAsk.h"
#include "Core/Agent/AgentIntegrations.h"
#include "Core/Agent/NoAgent.h"
#include "Core/Config/Config.h"
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/UI/Modals/ConfirmModal.h"
#include "Core/UI/Views/RootView.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

AgentAsk::~AgentAsk() {
    timer = nullptr;
}

std::string AgentAsk::Ask(const std::string &question) {
    auto agent = Editor::Instance().GetAgent();
    if (agent->Name() == NoAgent::NAME) {
        return "No agent set (main.ai_integration: none) - '.cfg set main.ai_integration claude' in your user config";
    }
    if (!agent->IsAvailable()) {
        return "Not asked - " + agent->Status();
    }
    if (question.empty()) {
        return "Usage: .ask <question> - put to the Editor Agent, after you OK it";
    }
    if (!RuntimeConfig::Instance().HasRootView()) {
        return "Not asked - no window";
    }

    // The id: the second it was asked, and a number for two in the same second
    auto now = std::chrono::system_clock::now();
    auto stamp = AgentQuestion::MakeId(now, 0);
    numberThisSecond = (stamp == lastIdStamp) ? numberThisSecond + 1 : 1;
    lastIdStamp = stamp;
    auto root = Editor::Instance().GetProjectRootPath();
    auto agentQuestion = AgentQuestion::Create(AgentQuestion::MakeId(now, numberThisSecond), question, AgentQuestion::QAFolder(root));
    agentQuestion->SetPrompt(agent->PromptWithAnswerFile(question, agentQuestion->AnswerFile()));

    bool willStart = (Editor::Instance().GetEditorAgentView() == nullptr);
    auto modal = ConfirmModal::Create("Ask the Editor Agent", UnicodeHelper::utf8to32(ConsentMessage(*agentQuestion, willStart)),
                                      {U"Send", U"Cancel"});
    modal->SetOnChoice([this, agentQuestion](int idxChoice) {
        OnConsent(agentQuestion, idxChoice == 0);
    });
    modal->SetOnCancel([this, agentQuestion]() {
        OnConsent(agentQuestion, false);
    });
    RuntimeConfig::Instance().GetRootViewAs<RootView>()->ShowModal(std::move(modal));
    return "Asking the Editor Agent - send it, or cancel";
}

// What will be typed, word for word, and to whom - wrapped to fit the modal
std::string AgentAsk::ConsentMessage(const AgentQuestion &question, bool willStart) const {
    auto agent = Editor::Instance().GetAgent();
    std::string message = "To the Editor Agent (" + agent->DisplayName() + ", read-only)";
    message += willStart ? " - it is started first.\n" : ".\n";
    message += "This is typed into it, as is:\n\n";
    std::string text = question.Prompt();
    size_t start = 0;
    while (start <= text.size()) {
        auto end = text.find('\n', start);
        auto line = text.substr(start, (end == std::string::npos) ? std::string::npos : end - start);
        // Wrap at a space where there is one
        while (line.size() > CONSENT_WIDTH) {
            auto cut = line.rfind(' ', CONSENT_WIDTH);
            if ((cut == std::string::npos) || (cut == 0)) {
                cut = CONSENT_WIDTH;
            }
            message += "  " + line.substr(0, cut) + "\n";
            line = line.substr((line[cut] == ' ') ? cut + 1 : cut);
        }
        message += "  " + line + "\n";
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    message += "\nKept with its answer: .goatedit/agent_qa/" + question.QuestionFile().filename().string();
    return message;
}

void AgentAsk::OnConsent(const AgentQuestion::Ref &question, bool isAccepted) {
    if (!isAccepted) {
        question->Decline();
        Editor::Instance().ShowStatusMessage(U"Not asked");
        return;
    }
    // The agent first - the question file names its conversation
    if (Editor::Instance().GetEditorAgentView() == nullptr) {
        if (OpenAgentTab() == nullptr) {
            Editor::Instance().ShowStatusMessage(U"Not asked - the Editor Agent could not be started");
            return;
        }
    }
    std::string error;
    if (!WriteQuestionFile(*question, error)) {
        Editor::Instance().ShowStatusMessage(UnicodeHelper::utf8to32("Not asked - " + error));
        return;
    }
    question->Accept(AgentQuestion::Clock::now());
    questions.push_back(question);
    Editor::Instance().ShowStatusMessage(UnicodeHelper::utf8to32("Asked - " + question->QuestionFile().filename().string()));
    Tick();
}

bool AgentAsk::WriteQuestionFile(const AgentQuestion &question, std::string &outError) {
    std::error_code ec;
    std::filesystem::create_directories(question.QuestionFile().parent_path(), ec);
    if (ec) {
        outError = "can't create " + question.QuestionFile().parent_path().string() + ": " + ec.message();
        return false;
    }
    auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local = {};
    localtime_r(&time, &local);
    char askedAt[32] = {};
    std::strftime(askedAt, sizeof(askedAt), "%Y-%m-%d %H:%M:%S", &local);

    std::ofstream out(question.QuestionFile(), std::ios::binary | std::ios::trunc);
    if (!out) {
        outError = "can't write " + question.QuestionFile().string();
        return false;
    }
    out << question.QuestionFileText(Editor::Instance().GetAgent()->DisplayName(), sessionId, askedAt);
    return true;
}

std::string AgentAsk::StartAgent() {
    auto agent = Editor::Instance().GetAgent();
    if (agent->Name() == NoAgent::NAME) {
        return "No agent set (main.ai_integration: none) - '.cfg set main.ai_integration claude' in your user config";
    }
    if (!agent->IsAvailable()) {
        return "Not started - " + agent->Status();
    }
    if (Editor::Instance().GetEditorAgentView() != nullptr) {
        Editor::Instance().ShowEditorAgentTab();
        return "The Editor Agent runs already - shown";
    }
    if (OpenAgentTab() == nullptr) {
        return "No panel to start it in, or " + agent->DisplayName() + " has no Editor Agent command";
    }
    Editor::Instance().ShowEditorAgentTab();
    return "Editor Agent started (" + agent->DisplayName() + ", read-only)";
}

TerminalView *AgentAsk::OpenAgentTab() {
    auto agent = Editor::Instance().GetAgent();
    auto newSessionId = AgentIntegrations::NewSessionId();
    auto command = agent->QuestionAgentCommand(Editor::Instance().GetProjectRootPath(), newSessionId);
    if (command.empty()) {
        return nullptr;
    }
    auto settings = AgentIntegrations::EditorAgentSettings(Config::Instance().ResolveSection("terminal"), command);
    auto newBanner = AgentIntegrations::EditorAgentBanner(*agent, command);
    // Not brought to front: the user asked from where they are; the status line says what it does
    auto *view = Editor::Instance().AddEditorAgentTab(settings, newBanner, false);
    if (view == nullptr) {
        return nullptr;
    }
    startedView = view;
    sessionId = newSessionId;
    banner = newBanner;
    isShowingWaitLine = false;
    ArmTimer();
    return view;
}

void AgentAsk::Tick() {
    auto *agentView = Editor::Instance().GetEditorAgentView();
    if (agentView != startedView) {
        // Its tab was closed: that conversation is over
        startedView = agentView;
        if (agentView == nullptr) {
            sessionId.clear();
        }
    }
    AgentQuestion::Signals signals;
    signals.now = AgentQuestion::Clock::now();
    signals.isTabOpen = (agentView != nullptr);
    if (agentView != nullptr) {
        auto &controller = agentView->GetController();
        signals.isAgentUp = controller.HasRunningJob();
        signals.isSettled = controller.IsSettled(SettleTime());
        signals.isTabFocused = controller.HasFocus();
    }

    bool isWaitingForFocus = false;
    while (!questions.empty()) {
        auto question = questions.front();
        signals.hasAnswerFile = std::filesystem::exists(question->AnswerFile());
        auto step = question->Update(signals, SettleTime());
        if (step == AgentQuestion::kStep::Send) {
            Send(*agentView, *question);
        } else if (step == AgentQuestion::kStep::Answered) {
            OnAnswered(*question);
        } else if (step == AgentQuestion::kStep::NoAnswer) {
            Editor::Instance().ShowStatusMessage(UnicodeHelper::utf8to32("No answer file for " + question->Id() +
                                                                         " - see the Editor Agent tab"));
        } else if (step == AgentQuestion::kStep::Cancelled) {
            Editor::Instance().ShowStatusMessage(UnicodeHelper::utf8to32("Not sent - the Editor Agent didn't run (" +
                                                                         question->Id() + ")"));
        }
        if (!question->IsDone()) {
            isWaitingForFocus = (question->State() == AgentQuestion::kState::WaitingToSend) && signals.isAgentUp &&
                                signals.isSettled && signals.isTabFocused;
            break;
        }
        questions.pop_front();
        // The next one waits for the agent to settle again - not this tick
        signals.isSettled = false;
    }
    UpdateInfoLine(agentView, isWaitingForFocus);

    auto status = ComputeStatus(signals, isWaitingForFocus);
    if (status != lastStatus) {
        lastStatus = status;
        Editor::Instance().TriggerUIRedraw();
    }
    if (!questions.empty() || (agentView != nullptr)) {
        ArmTimer();
    }
}

void AgentAsk::Send(TerminalView &agentView, const AgentQuestion &question) {
    // A bracketed paste (the agent turned it on - IsSettled checks), then Enter: the prompt's line breaks stay in it
    agentView.GetController().Promote(UnicodeHelper::utf8to32(question.Prompt()), true);
    Editor::Instance().TriggerUIRedraw();
}

void AgentAsk::OnAnswered(const AgentQuestion &question) {
    auto workspace = Editor::Instance().GetWorkspace();
    auto node = (workspace != nullptr) ? workspace->NewDocumentWithFileRef(question.AnswerFile()) : nullptr;
    if ((node == nullptr) || (Editor::Instance().OpenDocumentFromWorkspace(node) == nullptr)) {
        Editor::Instance().ShowStatusMessage(UnicodeHelper::utf8to32("Answered - can't open " + question.AnswerFile().string()));
        return;
    }
    Editor::Instance().ShowStatusMessage(UnicodeHelper::utf8to32("Answered - " + question.AnswerFile().filename().string()));
}

void AgentAsk::UpdateInfoLine(TerminalView *agentView, bool isWaitingForFocus) {
    if ((agentView == nullptr) || (isWaitingForFocus == isShowingWaitLine)) {
        return;
    }
    isShowingWaitLine = isWaitingForFocus;
    if (isWaitingForFocus) {
        Editor::Instance().SetEditorAgentInfoLine(U"A question from GoatEdit waits - it is typed in when you leave this tab");
    } else {
        Editor::Instance().SetEditorAgentInfoLine(UnicodeHelper::utf8to32(banner));
    }
}

// Worked out on each tick - the status line only reads it
std::u32string AgentAsk::ComputeStatus(const AgentQuestion::Signals &signals, bool isWaitingForFocus) const {
    std::u32string text;
    if (!questions.empty()) {
        auto &question = questions.front();
        switch (question->State()) {
            case AgentQuestion::kState::WaitingToSend :
                if (isWaitingForFocus) {
                    text = U"EA: waiting - leave its tab";
                } else {
                    text = signals.isAgentUp ? U"EA: waiting to send" : U"EA: starting";
                }
                break;
            case AgentQuestion::kState::Sent :
                text = U"EA: answering";
                break;
            default:
                text = U"EA: asking";
                break;
        }
        if (questions.size() > 1) {
            text += UnicodeHelper::utf8to32(fmt::format(" (+{})", questions.size() - 1));
        }
        return text;
    }
    if (!signals.isTabOpen) {
        return {};
    }
    if (!signals.isAgentUp) {
        return U"EA: stopped";
    }
    return signals.isSettled ? U"EA: idle" : U"EA: working";
}

std::chrono::milliseconds AgentAsk::SettleTime() const {
    auto fallback = Editor::Instance().GetAgent()->SettleTime();
    auto ms = Config::Instance()["main"].GetInt("ai_settle_ms", (int)fallback.count());
    return std::chrono::milliseconds((ms > 0) ? ms : 0);
}

void AgentAsk::ArmTimer() {
    if (timer != nullptr) {
        timer->Restart();
        return;
    }
    timer = Timer::Create(TICK, []() {
        if (!RuntimeConfig::Instance().HasRootView()) {
            return;
        }
        RuntimeConfig::Instance().GetRootView().PostMessage([]() {
            Editor::Instance().GetAgentAsk().Tick();
        });
    });
}

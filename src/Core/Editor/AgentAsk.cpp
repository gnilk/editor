//
// See AgentAsk.h
//

#include <ctime>
#include <fstream>
#include <sstream>

#include <fmt/format.h>

#include "AgentAsk.h"
#include "Core/Agent/AgentIntegrations.h"
#include "Core/Agent/NoAgent.h"
#include "Core/Config/Config.h"
#include "Core/Editor.h"
#include "Core/InstallPaths.h"
#include "Core/RuntimeConfig.h"
#include "Core/UI/Modals/ConfirmModal.h"
#include "Core/UI/Views/RootView.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

AgentAsk::~AgentAsk() {
    timer = nullptr;
}

static std::vector<std::string> Words(const std::string &text) {
    std::vector<std::string> words;
    std::istringstream in(text);
    std::string word;
    while (in >> word) {
        words.push_back(word);
    }
    return words;
}

static bool IsWordChar(char32_t ch) {
    return ((ch >= U'a') && (ch <= U'z')) || ((ch >= U'A') && (ch <= U'Z')) || ((ch >= U'0') && (ch <= U'9')) || (ch == U'_');
}

// The identifier under (or just before) the caret - empty when there is none
static std::string WordAt(const std::u32string &text, size_t idxChar) {
    if (idxChar > text.size()) {
        idxChar = text.size();
    }
    if (((idxChar == text.size()) || !IsWordChar(text[idxChar])) && (idxChar > 0) && IsWordChar(text[idxChar - 1])) {
        idxChar--;
    }
    if ((idxChar >= text.size()) || !IsWordChar(text[idxChar])) {
        return {};
    }
    size_t start = idxChar;
    while ((start > 0) && IsWordChar(text[start - 1])) {
        start--;
    }
    size_t end = idxChar;
    while ((end < text.size()) && IsWordChar(text[end])) {
        end++;
    }
    return UnicodeHelper::utf32to8(text.substr(start, end - start));
}

std::vector<std::string> AgentAsk::Ask(const std::string &text) {
    auto words = Words(text);
    if (words.empty()) {
        return {"Usage: .ask <question> | .ask <template> [args] | .ask list - put to the Editor Agent, after you OK it"};
    }
    if ((words.size() == 1) && (words.front() == "list")) {
        return ListTemplates();
    }
    if (auto blocker = AskBlocker(); !blocker.empty()) {
        return {blocker};
    }
    // A template when the first word names one and the rest fit its args - '.ask why is this slow' is a question
    std::vector<std::string> errors;
    auto templates = LoadTemplates(errors);
    if (auto questionTemplate = AgentQuestionTemplate::Find(templates, words.front()); questionTemplate != nullptr) {
        std::vector<std::string> templateWords(words.begin() + 1, words.end());
        if (templateWords.size() <= questionTemplate->Args().size()) {
            return {AskFromTemplate(questionTemplate, templateWords)};
        }
    }
    auto agent = Editor::Instance().GetAgent();
    auto agentQuestion = NewQuestion(text);
    agentQuestion->SetPrompt(agent->PromptWithAnswerFile(text, agentQuestion->AnswerFile()));
    ShowConsent(agentQuestion);
    return {"Asking the Editor Agent - send it, or cancel"};
}

std::string AgentAsk::AskBlocker() const {
    auto agent = Editor::Instance().GetAgent();
    if (agent->Name() == NoAgent::NAME) {
        return "No agent set (main.ai_integration: none) - '.cfg set main.ai_integration claude' in your user config";
    }
    if (!agent->IsAvailable()) {
        return "Not asked - " + agent->Status();
    }
    if (Editor::Instance().GetEditorAgentView() == nullptr) {
        if (auto blocker = agent->StartBlocker(Editor::Instance().GetProjectRootPath()); !blocker.empty()) {
            return "Not asked - " + blocker;
        }
    }
    if (!RuntimeConfig::Instance().HasRootView()) {
        return "Not asked - no window";
    }
    return {};
}

// System 'Questions/', then the user's 'questions/' - never the project's: a template runs commands
std::vector<AgentQuestionTemplate::Ref> AgentAsk::LoadTemplates(std::vector<std::string> &outErrors) const {
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto folders = assetLoader.ResolveExistingDirectories("Questions", AssetLoaderBase::kLocationType::kSystem);
    auto userFolders = assetLoader.ResolveExistingDirectories("questions", AssetLoaderBase::kLocationType::kUser);
    // The user's replace the system's: the later folder wins, so the user's go last - and of those, the preferred
    // (first) user dir last of all
    folders.insert(folders.end(), userFolders.rbegin(), userFolders.rend());
    return AgentQuestionTemplate::LoadAll(folders, outErrors);
}

std::vector<std::string> AgentAsk::ListTemplates() const {
    std::vector<std::string> errors;
    auto templates = LoadTemplates(errors);
    std::vector<std::string> lines;
    if (templates.empty()) {
        lines.emplace_back("No question templates (Questions/ in the system assets, questions/ in your user config)");
    }
    for (auto &questionTemplate : templates) {
        std::string about;
        for (auto &need : questionTemplate->Needs()) {
            about += (about.empty() ? "" : ", ") + need;
        }
        lines.push_back(fmt::format("  {:<12} {}{}", questionTemplate->Name(), questionTemplate->Title(), about.empty() ? "" : "  (at the caret: " + about + ")"));
        if (auto usage = questionTemplate->Usage(); !usage.empty()) {
            lines.push_back(fmt::format("  {:<12}   .ask {} {}", "", questionTemplate->Name(), usage));
        }
    }
    for (auto &error : errors) {
        lines.push_back("  not a template - " + error);
    }
    return lines;
}

std::string AgentAsk::AskFromTemplate(const AgentQuestionTemplate::Ref &questionTemplate, const std::vector<std::string> &words) {
    if (gathering != nullptr) {
        return "Not asked - still gathering for '" + gathering->questionTemplate->Name() + "'";
    }
    auto root = Editor::Instance().GetProjectRootPath();
    AgentQuestionTemplate::Values values;
    values["root"] = root.string();
    values["goatedit"] = InstallPaths::ExecutablePath().string();
    questionTemplate->ArgValues(words, values);

    bool needsCaret = questionTemplate->Needs("file") || questionTemplate->Needs("line") || questionTemplate->Needs("symbol");
    if (needsCaret) {
        auto document = Editor::Instance().GetActiveDocument();
        if ((document == nullptr) || document->GetPath().empty()) {
            return "Not asked - '" + questionTemplate->Name() + "' is about the caret's place in a file; no file is open";
        }
        auto path = document->GetPath();
        auto relative = path.lexically_relative(root);
        bool isInRoot = !relative.empty() && (relative.begin()->string() != "..");
        values["file"] = isInRoot ? relative.string() : path.string();
        auto &lineCursor = document->GetLineCursor();
        values["line"] = std::to_string(lineCursor.idxActiveLine + 1);
        auto line = document->ActiveLine();
        values["symbol"] = (line != nullptr) ? WordAt(line->Buffer(), lineCursor.cursor.position.x) : "";
        if (questionTemplate->Needs("symbol") && values["symbol"].empty()) {
            return "Not asked - '" + questionTemplate->Name() + "' is about the name under the caret; there is none";
        }
    }

    auto newGathering = std::make_shared<Gathering>();
    newGathering->questionTemplate = questionTemplate;
    newGathering->startedAt = AgentQuestion::Clock::now();
    newGathering->values = values;
    gathering = newGathering;
    ArmTimer();
    Tick();
    RunNextTool(newGathering);
    return "Gathering for '" + questionTemplate->Name() + "' - " + questionTemplate->Describe(values);
}

// One tool at a time, in the order the template lists them; the last one's exit fills the prompt
void AgentAsk::RunNextTool(const Gathering::Ref &toolGathering) {
    auto &tools = toolGathering->questionTemplate->Tools();
    if (toolGathering->idxTool >= tools.size()) {
        OnGathered(toolGathering);
        return;
    }
    auto &tool = tools[toolGathering->idxTool];
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto folders = assetLoader.ResolveExistingDirectories("Tools", AssetLoaderBase::kLocationType::kSystem);
    auto userFolders = assetLoader.ResolveExistingDirectories("tools", AssetLoaderBase::kLocationType::kUser);
    folders.insert(folders.end(), userFolders.rbegin(), userFolders.rend());
    auto toolFile = AgentQuestionTemplate::FindTool(folders, tool.command.front());
    if (toolFile.empty()) {
        gathering = nullptr;
        Editor::Instance().ShowStatusMessage(UnicodeHelper::utf8to32("Not asked - no tool '" + tool.command.front() +
                                                                     "' in Tools/ (system) or tools/ (user config)"));
        Tick();
        return;
    }
    auto command = AgentQuestionTemplate::ToolCommand(toolFile, toolGathering->questionTemplate->ToolArguments(tool, toolGathering->values));
    toolGathering->stdOut.clear();
    toolGathering->stdErr.clear();
    toolGathering->process = Process::Create(command.program, command.arguments);
    toolGathering->process->SetWorkingDirectory(Editor::Instance().GetProjectRootPath());
    // On the process's reader - each stream only appends to its own string, read after the exit
    toolGathering->process->SetOnStdOut([toolGathering](const std::string &data) {
        toolGathering->stdOut += data;
    });
    toolGathering->process->SetOnStdErr([toolGathering](const std::string &data) {
        toolGathering->stdErr += data;
    });
    toolGathering->process->SetOnExit([toolGathering](const Process::Result &result) {
        if (!RuntimeConfig::Instance().HasRootView()) {
            return;
        }
        RuntimeConfig::Instance().GetRootView().PostMessage([toolGathering, result]() {
            Editor::Instance().GetAgentAsk().OnToolExit(toolGathering, result);
        });
    });
    toolGathering->process->Start();
}

void AgentAsk::OnToolExit(const Gathering::Ref &toolGathering, const Process::Result &result) {
    if (toolGathering != gathering) {
        return;
    }
    auto &tool = toolGathering->questionTemplate->Tools()[toolGathering->idxTool];
    toolGathering->process = nullptr;
    if (!result.IsSuccess()) {
        gathering = nullptr;
        auto firstLine = toolGathering->stdErr.substr(0, toolGathering->stdErr.find('\n'));
        std::string why = (result.state == Process::kState::kFailedToStart) ? "didn't start: " + result.error :
                          fmt::format("failed (exit {})", result.exitCode) + (firstLine.empty() ? "" : ": " + firstLine);
        Editor::Instance().ShowStatusMessage(UnicodeHelper::utf8to32("Not asked - " + tool.command.front() + " " + why));
        Tick();
        return;
    }
    toolGathering->values["tool:" + tool.key] = AgentQuestionTemplate::CapOutput(toolGathering->stdOut, toolGathering->questionTemplate->MaxLines());
    toolGathering->idxTool++;
    RunNextTool(toolGathering);
}

void AgentAsk::OnGathered(const Gathering::Ref &doneGathering) {
    gathering = nullptr;
    auto &questionTemplate = doneGathering->questionTemplate;
    // The agent may have gone, or the window, while the tools ran
    if (auto blocker = AskBlocker(); !blocker.empty()) {
        Editor::Instance().ShowStatusMessage(UnicodeHelper::utf8to32(blocker));
        Tick();
        return;
    }
    auto agent = Editor::Instance().GetAgent();
    auto agentQuestion = NewQuestion(questionTemplate->Describe(doneGathering->values));
    auto filled = questionTemplate->FillPrompt(doneGathering->values);
    agentQuestion->SetPrompt(agent->PromptWithAnswerFile(filled, agentQuestion->AnswerFile()));
    // What it was asked about, for asking again: the caret's place, then the args
    std::string about;
    if (questionTemplate->Needs("file")) {
        about = doneGathering->values["file"] + (questionTemplate->Needs("line") ? ":" + doneGathering->values["line"] : "");
    }
    if (questionTemplate->Needs("symbol")) {
        about += (about.empty() ? "" : " ") + doneGathering->values["symbol"];
    }
    for (auto &arg : questionTemplate->Args()) {
        if (!doneGathering->values[arg].empty()) {
            about += (about.empty() ? "" : " ") + doneGathering->values[arg];
        }
    }
    agentQuestion->SetTemplate(questionTemplate->Name(), about);
    ShowConsent(agentQuestion);
    Tick();
}

AgentQuestion::Ref AgentAsk::NewQuestion(const std::string &question) {
    // The id: the second it was asked, and a number for two in the same second
    auto now = std::chrono::system_clock::now();
    auto stamp = AgentQuestion::MakeId(now, 0);
    numberThisSecond = (stamp == lastIdStamp) ? numberThisSecond + 1 : 1;
    lastIdStamp = stamp;
    auto root = Editor::Instance().GetProjectRootPath();
    return AgentQuestion::Create(AgentQuestion::MakeId(now, numberThisSecond), question, AgentQuestion::QAFolder(root));
}

void AgentAsk::ShowConsent(const AgentQuestion::Ref &agentQuestion) {
    bool willStart = (Editor::Instance().GetEditorAgentView() == nullptr);
    auto modal = ConfirmModal::Create("Ask the Editor Agent", UnicodeHelper::utf8to32(ConsentPrompt(*agentQuestion)),
                                      {U"Send", U"Cancel"});
    modal->SetHeader(UnicodeHelper::utf8to32(ConsentHeader(*agentQuestion, willStart)));
    modal->SetChoicePlacement(ConfirmModal::kChoicePlacement::kAbove);
    modal->SetOnChoice([this, agentQuestion](int idxChoice) {
        OnConsent(agentQuestion, idxChoice == 0);
    });
    modal->SetOnCancel([this, agentQuestion]() {
        OnConsent(agentQuestion, false);
    });
    RuntimeConfig::Instance().GetRootViewAs<RootView>()->ShowModal(std::move(modal));
}

// To whom, and where it is kept - above Send / Cancel, so a prompt taller than the screen never hides them
std::string AgentAsk::ConsentHeader(const AgentQuestion &question, bool willStart) const {
    auto agent = Editor::Instance().GetAgent();
    std::string header = "To the Editor Agent (" + agent->DisplayName() + ", read-only)";
    header += willStart ? " - it is started first.\n" : ".\n";
    header += "Kept with its answer: .goatedit/agent_qa/" + question.QuestionFile().filename().string() + "\n";
    header += "What is typed into it, as is, is below the line.";
    return header;
}

// What will be typed, word for word - wrapped to fit the modal
std::string AgentAsk::ConsentPrompt(const AgentQuestion &question) {
    std::string message;
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
    if (!message.empty() && (message.back() == '\n')) {
        message.pop_back();
    }
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
    if (auto blocker = agent->StartBlocker(Editor::Instance().GetProjectRootPath()); !blocker.empty()) {
        return "Not started - " + blocker;
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
    auto command = agent->QuestionAgentCommand(Editor::Instance().GetProjectRootPath(), newSessionId,
                                               Config::Instance()["main"].GetStr("ai_model", ""));
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
            question->SetTranscriptOffset(TranscriptSize());
            Send(*agentView, *question);
        } else if (step == AgentQuestion::kStep::Answered) {
            OnAnswered(*question, RecordUsage(*question));
        } else if (step == AgentQuestion::kStep::NoAnswer) {
            auto usage = RecordUsage(*question);
            Editor::Instance().ShowStatusMessage(UnicodeHelper::utf8to32("No answer file for " + question->Id() +
                                                                         " - see the Editor Agent tab" + usage));
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
    if (!questions.empty() || (agentView != nullptr) || (gathering != nullptr)) {
        ArmTimer();
    }
}

void AgentAsk::Send(TerminalView &agentView, const AgentQuestion &question) {
    // A bracketed paste (the agent turned it on - IsSettled checks), then Enter: the prompt's line breaks stay in it
    agentView.GetController().Promote(UnicodeHelper::utf8to32(question.Prompt()), true);
    Editor::Instance().TriggerUIRedraw();
}

void AgentAsk::OnAnswered(const AgentQuestion &question, const std::string &usage) {
    auto workspace = Editor::Instance().GetWorkspace();
    auto node = (workspace != nullptr) ? workspace->NewDocumentWithFileRef(question.AnswerFile()) : nullptr;
    if ((node == nullptr) || (Editor::Instance().OpenDocumentFromWorkspace(node) == nullptr)) {
        Editor::Instance().ShowStatusMessage(UnicodeHelper::utf8to32("Answered - can't open " + question.AnswerFile().string()));
        return;
    }
    Editor::Instance().ShowStatusMessage(UnicodeHelper::utf8to32("Answered - " + question.AnswerFile().filename().string() + usage));
}

// The size of this conversation's transcript now - 0 before the agent wrote one
uintmax_t AgentAsk::TranscriptSize() const {
    auto agent = Editor::Instance().GetAgent();
    auto transcript = agent->TranscriptFile(Editor::Instance().GetProjectRootPath(), sessionId);
    std::error_code ec;
    auto size = transcript.has_value() ? std::filesystem::file_size(transcript.value(), ec) : 0;
    return ec ? 0 : size;
}

// What the question cost, from the transcript: appended to its q_ file; ' (in ..., out ...)' for the status line, or
// empty when the agent keeps no usage
std::string AgentAsk::RecordUsage(const AgentQuestion &question) {
    auto agent = Editor::Instance().GetAgent();
    auto transcript = agent->TranscriptFile(Editor::Instance().GetProjectRootPath(), sessionId);
    if (!transcript.has_value()) {
        return {};
    }
    auto usage = agent->UsageSince(transcript.value(), question.TranscriptOffset());
    if (!usage.has_value() || (usage->numCalls == 0)) {
        return {};
    }
    std::ofstream out(question.QuestionFile(), std::ios::binary | std::ios::app);
    out << AgentQuestion::UsageText(usage.value());
    return " (" + AgentQuestion::UsageSummary(usage.value()) + ")";
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

// Worked out on each tick - the status line only reads it. While something takes a while (the tools, the agent
// starting, the answer) it moves: the indicator, and how long it has been
std::u32string AgentAsk::ComputeStatus(const AgentQuestion::Signals &signals, bool isWaitingForFocus) const {
    auto busy = [&signals](const std::u32string &what, AgentQuestion::Clock::time_point since) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(signals.now - since);
        return U"EA: " + BusyIndicator(elapsed) + U" " + what + U" " + Elapsed(elapsed);
    };
    std::u32string text;
    if (gathering != nullptr) {
        text = busy(U"gathering", gathering->startedAt);
        if (!questions.empty()) {
            text += UnicodeHelper::utf8to32(fmt::format(" (+{})", questions.size()));
        }
        return text;
    }
    if (!questions.empty()) {
        auto &question = questions.front();
        switch (question->State()) {
            case AgentQuestion::kState::WaitingToSend :
                if (isWaitingForFocus) {
                    text = U"EA: waiting - leave its tab";
                } else {
                    text = busy(signals.isAgentUp ? U"waiting to send" : U"starting", question->AcceptedAt());
                }
                break;
            case AgentQuestion::kState::Sent :
                text = busy(U"answering", question->SentAt());
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

std::u32string AgentAsk::BusyIndicator(std::chrono::milliseconds elapsed) {
    // The bright dot's place, there and back: 0 1 2 3 4 3 2 1
    static constexpr int WIDTH = 5;
    auto step = (int)((elapsed.count() / TICK.count()) % (2 * (WIDTH - 1)));
    auto at = (step < WIDTH) ? step : 2 * (WIDTH - 1) - step;
    std::u32string text;
    for (int i = 0; i < WIDTH; i++) {
        auto distance = std::abs(i - at);
        text += (distance == 0) ? U'●' : ((distance == 1) ? U'•' : U'·');
    }
    return text;
}

std::u32string AgentAsk::Elapsed(std::chrono::milliseconds elapsed) {
    auto seconds = (long long)std::max<long long>(0, elapsed.count() / 1000);
    if (seconds < 60) {
        return UnicodeHelper::utf8to32(fmt::format("{}s", seconds));
    }
    return UnicodeHelper::utf8to32(fmt::format("{}m {:02}s", seconds / 60, seconds % 60));
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

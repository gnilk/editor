//
// The agent integration (hitlist #78, docs/agent-integration.md): 'none' says there is nothing, 'claude' gives Claude
// Code's command lines and transcript folder, a name that isn't one makes nothing, and a project config can't pick one.
//
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <testinterface.h>

#include "Core/Agent/AgentIntegrations.h"
#include "Core/Agent/AgentQuestion.h"
#include "Core/Agent/ClaudeAgent.h"
#include "Core/Agent/NoAgent.h"
#include "Core/Config/Config.h"
#include "Core/Config/ConfigNode.h"
#include "Core/Config/ProjectConfigPolicy.h"
#include "Core/Editor.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_agent(ITesting *t);
DLL_EXPORT int test_agent_none(ITesting *t);
DLL_EXPORT int test_agent_create(ITesting *t);
DLL_EXPORT int test_agent_claude(ITesting *t);
DLL_EXPORT int test_agent_claude_transcripts(ITesting *t);
DLL_EXPORT int test_agent_find_executable(ITesting *t);
DLL_EXPORT int test_agent_project_config(ITesting *t);
DLL_EXPORT int test_agent_editor_default(ITesting *t);
DLL_EXPORT int test_agent_shell_line(ITesting *t);
DLL_EXPORT int test_agent_editor_agent_settings(ITesting *t);
DLL_EXPORT int test_agent_editor_agent_banner(ITesting *t);
DLL_EXPORT int test_agent_start_none(ITesting *t);
DLL_EXPORT int test_agent_session_id(ITesting *t);
DLL_EXPORT int test_agent_question_files(ITesting *t);
DLL_EXPORT int test_agent_question_round(ITesting *t);
DLL_EXPORT int test_agent_question_waits(ITesting *t);
DLL_EXPORT int test_agent_question_ends(ITesting *t);
DLL_EXPORT int test_agent_ask_none(ITesting *t);
}

DLL_EXPORT int test_agent(ITesting *t) {
    return kTR_Pass;
}

// 'none': no command, no transcripts, no prompt - nothing to start or send
DLL_EXPORT int test_agent_none(ITesting *t) {
    auto none = NoAgent::Create();
    TR_ASSERT(t, none->Name() == "none");
    TR_ASSERT(t, !none->IsAvailable());
    TR_ASSERT(t, none->WorkingAgentCommand("/p").empty());
    TR_ASSERT(t, none->QuestionAgentCommand("/p", "id").empty());
    TR_ASSERT(t, !none->TranscriptDir("/p").has_value());
    TR_ASSERT(t, none->PromptWithAnswerFile("why?", "/p/.goatedit/answer.md").empty());
    TR_ASSERT(t, !none->Status().empty());
    return kTR_Pass;
}

// By name, as main.ai_integration says; a name there is none of makes nothing (the editor says so and uses none)
DLL_EXPORT int test_agent_create(ITesting *t) {
    TR_ASSERT(t, AgentIntegrations::Create("none")->Name() == "none");
    TR_ASSERT(t, AgentIntegrations::Create("claude")->Name() == "claude");
    TR_ASSERT(t, AgentIntegrations::Create("Claude") == nullptr);
    TR_ASSERT(t, AgentIntegrations::Create("") == nullptr);
    TR_ASSERT(t, AgentIntegrations::Create("codex") == nullptr);
    for (auto &name : AgentIntegrations::Names()) {
        TR_ASSERT(t, AgentIntegrations::Create(name) != nullptr);
    }
    return kTR_Pass;
}

// Claude Code: 'claude' for the working agent; the question agent denies anything not allowed up front and may write
// only answer files - by an absolute rule ('//'), whatever the tab's directory - in the conversation the editor names.
// The prompt names the answer file and says it is the only file to change.
DLL_EXPORT int test_agent_claude(ITesting *t) {
    auto claude = ClaudeAgent::Create();
    TR_ASSERT(t, claude->DisplayName() == "Claude Code");
    TR_ASSERT(t, (claude->WorkingAgentCommand("/p") == std::vector<std::string>{"claude"}));
    TR_ASSERT(t, (claude->QuestionAgentCommand("/p", "1234") ==
                  std::vector<std::string>{"claude", "--permission-mode", "dontAsk",
                                           "--allowedTools", "Read Grep Glob Edit(//p/.goatedit/agent_qa/a_*)",
                                           "--session-id", "1234"}));
    // No id: the agent picks one
    TR_ASSERT(t, claude->QuestionAgentCommand("/p", "").size() == 5);
    TR_ASSERT(t, claude->SettleTime() == std::chrono::milliseconds(3000));
    auto prompt = claude->PromptWithAnswerFile("Why is X here?", "/p/.goatedit/answers/1.md");
    TR_ASSERT(t, prompt.find("Why is X here?") == 0);
    TR_ASSERT(t, prompt.find("/p/.goatedit/answers/1.md") != std::string::npos);
    TR_ASSERT(t, prompt.find("no other file") != std::string::npos);
    return kTR_Pass;
}

// Its transcripts: '<config dir>/projects/<the root, every non-alphanumeric as '-'>' - $CLAUDE_CONFIG_DIR wins over ~/.claude
DLL_EXPORT int test_agent_claude_transcripts(ITesting *t) {
    TR_ASSERT(t, ClaudeAgent::ProjectFolderName("/Users/a/src/github.com/editor") == "-Users-a-src-github-com-editor");
    TR_ASSERT(t, ClaudeAgent::ProjectFolderName("/home/a/my_proj/") == "-home-a-my-proj");
    TR_ASSERT(t, ClaudeAgent::ProjectFolderName("/home/a/x/../y") == "-home-a-y");

    auto saved = std::getenv("CLAUDE_CONFIG_DIR");
    std::string savedValue = (saved != nullptr) ? saved : "";
    setenv("CLAUDE_CONFIG_DIR", "/tmp/goatedit_agent_cfg", 1);
    auto dir = ClaudeAgent::Create()->TranscriptDir("/home/a/proj");
    TR_ASSERT(t, dir.has_value() && (dir.value() == std::filesystem::path("/tmp/goatedit_agent_cfg/projects/-home-a-proj")));
    unsetenv("CLAUDE_CONFIG_DIR");
    auto home = std::getenv("HOME");
    auto homeDir = ClaudeAgent::Create()->TranscriptDir("/home/a/proj");
    if (home != nullptr) {
        TR_ASSERT(t, homeDir.has_value() && (homeDir.value() == std::filesystem::path(home) / ".claude" / "projects" / "-home-a-proj"));
    }
    if (saved != nullptr) {
        setenv("CLAUDE_CONFIG_DIR", savedValue.c_str(), 1);
    }
    return kTR_Pass;
}

// On PATH first, then the extra places; only an executable file; a name with a '/' isn't looked up
DLL_EXPORT int test_agent_find_executable(ITesting *t) {
    auto dir = std::filesystem::temp_directory_path() / "goatedit_agent_find_executable";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "a");
    std::filesystem::create_directories(dir / "b");
    std::filesystem::create_directories(dir / "extra");
    {
        std::ofstream(dir / "a" / "tool") << "not executable";
        std::ofstream(dir / "b" / "tool") << "#!/bin/sh\n";
        std::ofstream(dir / "extra" / "other") << "#!/bin/sh\n";
    }
    std::filesystem::permissions(dir / "b" / "tool", std::filesystem::perms::owner_all);
    std::filesystem::permissions(dir / "extra" / "other", std::filesystem::perms::owner_all);
    auto path = (dir / "a").string() + "::" + (dir / "b").string();

    auto found = AgentIntegrations::FindExecutable("tool", path, {});
    TR_ASSERT(t, found.has_value() && (found.value() == dir / "b" / "tool"));
    TR_ASSERT(t, !AgentIntegrations::FindExecutable("other", path, {}).has_value());
    auto extra = AgentIntegrations::FindExecutable("other", path, {dir / "extra"});
    TR_ASSERT(t, extra.has_value() && (extra.value() == dir / "extra" / "other"));
    TR_ASSERT(t, !AgentIntegrations::FindExecutable("b/tool", dir.string(), {}).has_value());
    TR_ASSERT(t, !AgentIntegrations::FindExecutable("", path, {}).has_value());
    std::filesystem::remove_all(dir);
    return kTR_Pass;
}

// A project config can't pick the agent - agent calls cost money and run on the user's account
DLL_EXPORT int test_agent_project_config(ITesting *t) {
    auto project = ConfigNode::FromString("main:\n  ai_integration: claude\n  tabsize: 2\n");
    TR_ASSERT(t, project.has_value());
    auto removed = ProjectConfigPolicy::StripDeniedKeys(project.value());
    TR_ASSERT(t, std::find(removed.begin(), removed.end(), "main.ai_integration") != removed.end());
    TR_ASSERT(t, project->GetNode("main").GetStr("ai_integration", "gone") == "gone");
    TR_ASSERT(t, project->GetNode("main").GetInt("tabsize", 0) == 2);
    TR_ASSERT(t, ProjectConfigPolicy::IsDenied({"main", "ai_integration"}));
    return kTR_Pass;
}

// The editor has an agent always - the one the config names ('none' when it names none; a local config may say claude)
DLL_EXPORT int test_agent_editor_default(ITesting *t) {
    auto agent = Editor::Instance().GetAgent();
    TR_ASSERT(t, agent != nullptr);
    auto configured = Config::Instance()["main"].GetStr("ai_integration", NoAgent::NAME);
    auto expected = (AgentIntegrations::Create(configured) != nullptr) ? configured : std::string(NoAgent::NAME);
    TR_ASSERT(t, agent->Name() == expected);
    return kTR_Pass;
}

// The Editor Agent's command typed into its shell: plain words as they are, anything else single-quoted
DLL_EXPORT int test_agent_shell_line(ITesting *t) {
    TR_ASSERT(t, AgentIntegrations::ShellCommandLine({"claude", "--permission-mode", "plan"}) == "claude --permission-mode plan");
    TR_ASSERT(t, AgentIntegrations::ShellCommandLine({"/usr/bin/x", "a=b:c+d"}) == "/usr/bin/x a=b:c+d");
    TR_ASSERT(t, AgentIntegrations::ShellCommandLine({"a b", "$HOME", ""}) == "'a b' '$HOME' ''");
    TR_ASSERT(t, AgentIntegrations::ShellCommandLine({"it's"}) == "'it'\\''s'");
    TR_ASSERT(t, AgentIntegrations::ShellCommandLine({}).empty());
    return kTR_Pass;
}

// Its settings: the terminal section's (the shell), the bootstrap is the agent; not saved, not the promote target, not
// the output console - and the section it was made from is left as it was
DLL_EXPORT int test_agent_editor_agent_settings(ITesting *t) {
    auto terminal = ConfigNode::FromString("shell: /bin/zsh\nbootstrap: [ 'echo hi' ]\npromote_target: yes\n");
    TR_ASSERT(t, terminal.has_value());
    auto settings = AgentIntegrations::EditorAgentSettings(terminal.value(), {"claude", "--permission-mode", "plan"});
    TR_ASSERT(t, settings.GetStr("shell", "") == "/bin/zsh");
    TR_ASSERT(t, (settings.GetSequenceOfStr("bootstrap") == std::vector<std::string>{"claude --permission-mode plan"}));
    TR_ASSERT(t, !settings.GetBool("persist_scrollback", true));
    TR_ASSERT(t, !settings.GetBool("promote_target", true));
    TR_ASSERT(t, !settings.GetBool("output_console", true));
    TR_ASSERT(t, (terminal->GetSequenceOfStr("bootstrap") == std::vector<std::string>{"echo hi"}));
    TR_ASSERT(t, terminal->GetBool("promote_target", false));
    TR_ASSERT(t, !terminal->HasKey("output_console"));

    // No section at all: still a map with the agent in it
    auto bare = AgentIntegrations::EditorAgentSettings(ConfigNode(), {"claude"});
    TR_ASSERT(t, (bare.GetSequenceOfStr("bootstrap") == std::vector<std::string>{"claude"}));
    return kTR_Pass;
}

// The banner says whose it is and what it runs
DLL_EXPORT int test_agent_editor_agent_banner(ITesting *t) {
    auto claude = ClaudeAgent::Create();
    auto banner = AgentIntegrations::EditorAgentBanner(*claude, claude->QuestionAgentCommand("/p", "1234"));
    TR_ASSERT(t, banner.find("GoatEdit's own agent") == 0);
    TR_ASSERT(t, banner.find("Claude Code") != std::string::npos);
    TR_ASSERT(t, banner.find("claude --permission-mode dontAsk") != std::string::npos);
    TR_ASSERT(t, banner.find("'Read Grep Glob Edit(//p/.goatedit/agent_qa/a_*)'") != std::string::npos);
    TR_ASSERT(t, banner.find('\n') == std::string::npos);
    TR_ASSERT(t, banner.find("stops it") != std::string::npos);
    return kTR_Pass;
}

// With 'none' there is nothing to start - and nothing is; with an agent, the tests have no panel - nothing starts either
DLL_EXPORT int test_agent_start_none(ITesting *t) {
    auto message = Editor::Instance().StartEditorAgent();
    if (Editor::Instance().GetAgent()->Name() == NoAgent::NAME) {
        TR_ASSERT(t, message.find("none") != std::string::npos);
    } else {
        TR_ASSERT(t, message.find("Editor Agent started") == std::string::npos);
    }
    return kTR_Pass;
}

// A version 4 UUID, a new one each time
DLL_EXPORT int test_agent_session_id(ITesting *t) {
    auto first = AgentIntegrations::NewSessionId();
    auto second = AgentIntegrations::NewSessionId();
    TR_ASSERT(t, first != second);
    TR_ASSERT(t, first.size() == 36);
    TR_ASSERT(t, (first[8] == '-') && (first[13] == '-') && (first[18] == '-') && (first[23] == '-'));
    TR_ASSERT(t, first[14] == '4');
    TR_ASSERT(t, std::string("89ab").find(first[19]) != std::string::npos);
    return kTR_Pass;
}

// The question is kept with its answer: q_<id>.md / a_<id>.md in .goatedit/agent_qa, the q file holds the question,
// who was asked and the prompt as sent
DLL_EXPORT int test_agent_question_files(ITesting *t) {
    std::tm when = {};
    when.tm_year = 2026 - 1900;
    when.tm_mon = 10;
    when.tm_mday = 24;
    when.tm_hour = 14;
    when.tm_min = 3;
    when.tm_sec = 12;
    when.tm_isdst = -1;
    auto at = std::chrono::system_clock::from_time_t(std::mktime(&when));
    TR_ASSERT(t, AgentQuestion::MakeId(at, 2) == "20261124-140312-2");

    auto folder = AgentQuestion::QAFolder("/p");
    TR_ASSERT(t, folder == std::filesystem::path("/p/.goatedit/agent_qa"));
    auto question = AgentQuestion::Create("20261124-140312-2", "Why is X here?", folder);
    TR_ASSERT(t, question->QuestionFile() == folder / "q_20261124-140312-2.md");
    TR_ASSERT(t, question->AnswerFile() == folder / "a_20261124-140312-2.md");
    question->SetPrompt("Why is X here?\n\nWrite your answer to a_20261124-140312-2.md");
    auto text = question->QuestionFileText("Claude Code", "uuid-1", "2026-11-24 14:03:12");
    TR_ASSERT(t, text.find("---\n") == 0);
    TR_ASSERT(t, text.find("agent: Claude Code\n") != std::string::npos);
    TR_ASSERT(t, text.find("session: uuid-1\n") != std::string::npos);
    TR_ASSERT(t, text.find("answer: a_20261124-140312-2.md\n") != std::string::npos);
    TR_ASSERT(t, text.find("# Question\n\nWhy is X here?\n") != std::string::npos);
    TR_ASSERT(t, text.find("```text\nWhy is X here?\n\nWrite your answer") != std::string::npos);
    return kTR_Pass;
}

static AgentQuestion::Signals AgentSignals(AgentQuestion::Clock::time_point now, bool isUp, bool isSettled, bool isFocused, bool hasAnswer) {
    AgentQuestion::Signals signals;
    signals.now = now;
    signals.isTabOpen = true;
    signals.isAgentUp = isUp;
    signals.isSettled = isSettled;
    signals.isTabFocused = isFocused;
    signals.hasAnswerFile = hasAnswer;
    return signals;
}

// All the way round: nothing before consent; sent once the agent is up and settled; answered once the file is there and
// the agent has settled again - not in the quiet moment right after Enter
DLL_EXPORT int test_agent_question_round(ITesting *t) {
    using namespace std::chrono_literals;
    auto settle = std::chrono::milliseconds(3000);
    auto t0 = AgentQuestion::Clock::now();
    auto question = AgentQuestion::Create("1", "q", "/p/.goatedit/agent_qa");
    TR_ASSERT(t, question->State() == AgentQuestion::kState::Consent);
    TR_ASSERT(t, question->Update(AgentSignals(t0, true, true, false, false), settle) == AgentQuestion::kStep::None);

    question->Accept(t0);
    TR_ASSERT(t, question->State() == AgentQuestion::kState::WaitingToSend);
    // Starting: not up yet, then up but still printing
    TR_ASSERT(t, question->Update(AgentSignals(t0 + 1s, false, false, false, false), settle) == AgentQuestion::kStep::None);
    TR_ASSERT(t, question->Update(AgentSignals(t0 + 2s, true, false, false, false), settle) == AgentQuestion::kStep::None);
    TR_ASSERT(t, question->Update(AgentSignals(t0 + 5s, true, true, false, false), settle) == AgentQuestion::kStep::Send);
    TR_ASSERT(t, question->State() == AgentQuestion::kState::Sent);

    // Right after Enter: quiet (the settle window hasn't passed), even with a file there - not yet
    TR_ASSERT(t, question->Update(AgentSignals(t0 + 6s, true, true, false, true), settle) == AgentQuestion::kStep::None);
    // Working
    TR_ASSERT(t, question->Update(AgentSignals(t0 + 20s, true, false, false, true), settle) == AgentQuestion::kStep::None);
    // Settled with the file
    TR_ASSERT(t, question->Update(AgentSignals(t0 + 30s, true, true, false, true), settle) == AgentQuestion::kStep::Answered);
    TR_ASSERT(t, question->IsDone());
    TR_ASSERT(t, question->Update(AgentSignals(t0 + 31s, true, true, false, true), settle) == AgentQuestion::kStep::None);
    return kTR_Pass;
}

// Nothing is typed in while the user is in the agent's tab; it goes when they leave
DLL_EXPORT int test_agent_question_waits(ITesting *t) {
    using namespace std::chrono_literals;
    auto settle = std::chrono::milliseconds(3000);
    auto t0 = AgentQuestion::Clock::now();
    auto question = AgentQuestion::Create("1", "q", "/p/.goatedit/agent_qa");
    question->Accept(t0);
    TR_ASSERT(t, question->Update(AgentSignals(t0 + 5s, true, true, true, false), settle) == AgentQuestion::kStep::None);
    TR_ASSERT(t, question->Update(AgentSignals(t0 + 50s, true, true, true, false), settle) == AgentQuestion::kStep::None);
    TR_ASSERT(t, question->State() == AgentQuestion::kState::WaitingToSend);
    TR_ASSERT(t, question->Update(AgentSignals(t0 + 51s, true, true, false, false), settle) == AgentQuestion::kStep::Send);
    return kTR_Pass;
}

// The ways it ends without an answer: declined; the tab closed before sending; the agent never came up; settled without
// writing the file; the agent stopped after it was sent (a file it wrote still counts)
DLL_EXPORT int test_agent_question_ends(ITesting *t) {
    using namespace std::chrono_literals;
    auto settle = std::chrono::milliseconds(3000);
    auto t0 = AgentQuestion::Clock::now();

    auto declined = AgentQuestion::Create("1", "q", "/p");
    declined->Decline();
    TR_ASSERT(t, declined->State() == AgentQuestion::kState::Cancelled);

    auto closed = AgentQuestion::Create("2", "q", "/p");
    closed->Accept(t0);
    auto noTab = AgentSignals(t0 + 1s, false, false, false, false);
    noTab.isTabOpen = false;
    TR_ASSERT(t, closed->Update(noTab, settle) == AgentQuestion::kStep::Cancelled);

    auto neverUp = AgentQuestion::Create("3", "q", "/p");
    neverUp->Accept(t0);
    TR_ASSERT(t, neverUp->Update(AgentSignals(t0 + 59s, false, true, false, false), settle) == AgentQuestion::kStep::None);
    TR_ASSERT(t, neverUp->Update(AgentSignals(t0 + 61s, false, true, false, false), settle) == AgentQuestion::kStep::Cancelled);

    auto silent = AgentQuestion::Create("4", "q", "/p");
    silent->Accept(t0);
    TR_ASSERT(t, silent->Update(AgentSignals(t0 + 4s, true, true, false, false), settle) == AgentQuestion::kStep::Send);
    TR_ASSERT(t, silent->Update(AgentSignals(t0 + 10s, true, true, false, false), settle) == AgentQuestion::kStep::NoAnswer);

    auto stopped = AgentQuestion::Create("5", "q", "/p");
    stopped->Accept(t0);
    TR_ASSERT(t, stopped->Update(AgentSignals(t0 + 4s, true, true, false, false), settle) == AgentQuestion::kStep::Send);
    TR_ASSERT(t, stopped->Update(AgentSignals(t0 + 5s, false, false, false, true), settle) == AgentQuestion::kStep::Answered);
    return kTR_Pass;
}

// '.ask' with 'none' says so; with an agent the tests have no window - nothing is asked either way
DLL_EXPORT int test_agent_ask_none(ITesting *t) {
    auto message = Editor::Instance().AskEditorAgent("why?");
    TR_ASSERT(t, message.find("Asking") == std::string::npos);
    if (Editor::Instance().GetAgent()->Name() == NoAgent::NAME) {
        TR_ASSERT(t, message.find("none") != std::string::npos);
    }
    return kTR_Pass;
}

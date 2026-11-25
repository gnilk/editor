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
#include "Core/Agent/AgentQuestionTemplate.h"
#include "Core/Agent/ClaudeAgent.h"
#include "Core/Agent/NoAgent.h"
#include "Core/Config/Config.h"
#include "Core/Config/ConfigNode.h"
#include "Core/Config/ProjectConfigPolicy.h"
#include "Core/Editor.h"
#include "Core/Editor/AgentAsk.h"
#include "Core/RuntimeConfig.h"

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
DLL_EXPORT int test_agent_claude_usage(ITesting *t);
DLL_EXPORT int test_agent_claude_trusted(ITesting *t);
DLL_EXPORT int test_agent_template_parse(ITesting *t);
DLL_EXPORT int test_agent_template_errors(ITesting *t);
DLL_EXPORT int test_agent_template_fill(ITesting *t);
DLL_EXPORT int test_agent_template_tool(ITesting *t);
DLL_EXPORT int test_agent_template_cap(ITesting *t);
DLL_EXPORT int test_agent_template_folders(ITesting *t);
DLL_EXPORT int test_agent_template_shipped(ITesting *t);
DLL_EXPORT int test_agent_question_template_file(ITesting *t);
DLL_EXPORT int test_agent_busy_indicator(ITesting *t);
DLL_EXPORT int test_agent_model_name(ITesting *t);
DLL_EXPORT int test_agent_model_none(ITesting *t);
DLL_EXPORT int test_agent_unsaved_summary(ITesting *t);
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
    TR_ASSERT(t, none->QuestionAgentCommand("/p", "id", "").empty());
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
    TR_ASSERT(t, (claude->QuestionAgentCommand("/p", "1234", "") ==
                  std::vector<std::string>{"claude", "--permission-mode", "dontAsk",
                                           "--allowedTools", "Read Grep Glob Edit(//p/.goatedit/agent_qa/a_*)",
                                           "--session-id", "1234"}));
    // No id: the agent picks one
    TR_ASSERT(t, claude->QuestionAgentCommand("/p", "", "").size() == 5);
    // A model asked for
    auto withModel = claude->QuestionAgentCommand("/p", "", "sonnet");
    TR_ASSERT(t, (withModel.size() == 7) && (withModel[5] == "--model") && (withModel[6] == "sonnet"));
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
    auto banner = AgentIntegrations::EditorAgentBanner(*claude, claude->QuestionAgentCommand("/p", "1234", ""));
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
    auto lines = Editor::Instance().AskEditorAgent("why?");
    TR_ASSERT(t, lines.size() == 1);
    auto &message = lines.front();
    TR_ASSERT(t, message.find("Asking") == std::string::npos);
    if (Editor::Instance().GetAgent()->Name() == NoAgent::NAME) {
        TR_ASSERT(t, message.find("none") != std::string::npos);
    }
    return kTR_Pass;
}

// What a question cost, from Claude Code's transcript: assistant lines after the offset, each message counted once (it is
// written as one line per content block, every line with the same usage); other lines and broken ones are skipped
DLL_EXPORT int test_agent_claude_usage(ITesting *t) {
    auto folder = std::filesystem::temp_directory_path() / "goatedit_agent_usage";
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    auto transcript = folder / "session.jsonl";
    std::string before = R"({"type":"assistant","message":{"id":"m0","model":"x","usage":{"input_tokens":1000,"output_tokens":1000}}})" "\n";
    {
        std::ofstream out(transcript, std::ios::binary);
        out << before;
        out << R"({"type":"user","message":{"role":"user","content":"why?"}})" "\n";
        out << R"({"type":"assistant","message":{"id":"m1","model":"claude-sonnet-5-5","usage":{"input_tokens":3,"cache_creation_input_tokens":30000,"cache_read_input_tokens":0,"output_tokens":40}}})" "\n";
        out << R"({"type":"assistant","message":{"id":"m1","model":"claude-sonnet-5-5","usage":{"input_tokens":3,"cache_creation_input_tokens":30000,"cache_read_input_tokens":0,"output_tokens":40}}})" "\n";
        out << "not json\n";
        out << R"({"type":"assistant","message":{"id":"m2","model":"claude-sonnet-5-5","usage":{"input_tokens":2,"cache_creation_input_tokens":500,"cache_read_input_tokens":30000,"output_tokens":200}}})" "\n";
    }
    auto claude = ClaudeAgent::Create();
    auto usage = claude->UsageSince(transcript, before.size());
    TR_ASSERT(t, usage.has_value());
    TR_ASSERT(t, usage->numCalls == 2);
    TR_ASSERT(t, usage->inputTokens == 5);
    TR_ASSERT(t, usage->cacheWriteTokens == 30500);
    TR_ASSERT(t, usage->cacheReadTokens == 30000);
    TR_ASSERT(t, usage->outputTokens == 240);
    TR_ASSERT(t, usage->model == "claude-sonnet-5-5");
    TR_ASSERT(t, AgentQuestion::UsageSummary(usage.value()) == "in 31k + 30k cached, out 240");
    TR_ASSERT(t, AgentQuestion::UsageText(usage.value()).find("- output tokens: 240") != std::string::npos);
    TR_ASSERT(t, claude->UsageSince(transcript, 0)->numCalls == 3);
    TR_ASSERT(t, !claude->UsageSince(folder / "none.jsonl", 0).has_value());
    TR_ASSERT(t, (claude->TranscriptFile("/p", "abc").value().filename() == "abc.jsonl"));
    std::filesystem::remove_all(folder);
    return kTR_Pass;
}

// Trusted in the folder or one above it, by Claude Code's own config - else the Editor Agent isn't started
DLL_EXPORT int test_agent_claude_trusted(ITesting *t) {
    auto folder = std::filesystem::temp_directory_path() / "goatedit_agent_trusted";
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    auto config = folder / ".claude.json";
    {
        std::ofstream out(config, std::ios::binary);
        out << R"({"projects": {"/a/trusted": {"hasTrustDialogAccepted": true}, "/a/asked": {"hasTrustDialogAccepted": false}}})";
    }
    TR_ASSERT(t, ClaudeAgent::IsTrusted("/a/trusted", config));
    TR_ASSERT(t, ClaudeAgent::IsTrusted("/a/trusted/", config));
    TR_ASSERT(t, ClaudeAgent::IsTrusted("/a/trusted/sub/dir", config));
    TR_ASSERT(t, !ClaudeAgent::IsTrusted("/a/asked", config));
    TR_ASSERT(t, !ClaudeAgent::IsTrusted("/a", config));
    TR_ASSERT(t, !ClaudeAgent::IsTrusted("/a/trusted", folder / "missing.json"));
    std::filesystem::remove_all(folder);
    return kTR_Pass;
}

//
// Questions from templates (agent-integration.md §6.4)
//
static const char *WHY_TEMPLATE = "---\n"
                                  "name: why\n"
                                  "title: Why is this line here?\n"
                                  "needs: [file, line]\n"
                                  "tools:\n"
                                  "  chain: why.py {file}:{line}\n"
                                  "max_lines: 50\n"
                                  "---\n"
                                  "\n"
                                  "Line {line} of {file}:\n"
                                  "{tool:chain}\n"
                                  "Code stays: if (x) { return; } - and {{line} is literal\n";

// The front matter and the prompt after it - leading blank lines are not the prompt
DLL_EXPORT int test_agent_template_parse(ITesting *t) {
    std::string error;
    auto why = AgentQuestionTemplate::FromText(WHY_TEMPLATE, error);
    TR_ASSERT(t, why != nullptr);
    TR_ASSERT(t, why->Name() == "why");
    TR_ASSERT(t, why->Title() == "Why is this line here?");
    TR_ASSERT(t, why->Needs("file") && why->Needs("line") && !why->Needs("symbol"));
    TR_ASSERT(t, why->Args().empty());
    TR_ASSERT(t, why->MaxLines() == 50);
    TR_ASSERT(t, why->Tools().size() == 1);
    TR_ASSERT(t, why->Tools()[0].key == "chain");
    TR_ASSERT(t, (why->Tools()[0].command == std::vector<std::string>{"why.py", "{file}:{line}"}));
    TR_ASSERT(t, why->FillPrompt({}).rfind("Line ", 0) == 0);

    // No tools, no needs - a plain prompt with an optional arg; max_lines defaults
    auto plain = AgentQuestionTemplate::FromText("---\nname: plain\nargs: [base]\n---\nAgainst {base}.\n", error);
    TR_ASSERT(t, plain != nullptr);
    TR_ASSERT(t, plain->Usage() == "[base]");
    TR_ASSERT(t, why->Usage().empty());
    auto described = AgentQuestionTemplate::FromText("---\nname: d\nargs: [revision]\nusage: \"[git revision]\"\n---\nhi\n", error);
    TR_ASSERT(t, described->Usage() == "[git revision]");
    TR_ASSERT(t, plain->Tools().empty());
    TR_ASSERT(t, plain->MaxLines() == AgentQuestionTemplate::DEFAULT_MAX_LINES);
    return kTR_Pass;
}

// A typo never reaches the agent: what isn't a template says why
DLL_EXPORT int test_agent_template_errors(ITesting *t) {
    std::string error;
    TR_ASSERT(t, AgentQuestionTemplate::FromText("name: x\n---\nhi\n", error) == nullptr);
    TR_ASSERT(t, error.find("front matter") != std::string::npos);
    TR_ASSERT(t, AgentQuestionTemplate::FromText("---\nname: x\nhi\n", error) == nullptr);
    TR_ASSERT(t, AgentQuestionTemplate::FromText("---\ntitle: no name\n---\nhi\n", error) == nullptr);
    TR_ASSERT(t, error.find("name") != std::string::npos);
    TR_ASSERT(t, AgentQuestionTemplate::FromText("---\nname: Why Not\n---\nhi\n", error) == nullptr);
    TR_ASSERT(t, AgentQuestionTemplate::FromText("---\nname: x\n---\n\n\n", error) == nullptr);
    TR_ASSERT(t, error.find("prompt") != std::string::npos);
    // Placeholders: in the prompt, in a tool's arguments, a tool key that isn't there, a need that doesn't exist
    TR_ASSERT(t, AgentQuestionTemplate::FromText("---\nname: x\nneeds: [file]\n---\n{flie}\n", error) == nullptr);
    TR_ASSERT(t, error.find("{flie}") != std::string::npos);
    TR_ASSERT(t, AgentQuestionTemplate::FromText("---\nname: x\ntools:\n  a: t.py {line}\n---\n{tool:a}\n", error) == nullptr);
    TR_ASSERT(t, error.find("'a'") != std::string::npos);
    TR_ASSERT(t, AgentQuestionTemplate::FromText("---\nname: x\ntools:\n  a: t.py\n---\n{tool:b}\n", error) == nullptr);
    TR_ASSERT(t, AgentQuestionTemplate::FromText("---\nname: x\nneeds: [selection]\n---\nhi\n", error) == nullptr);
    TR_ASSERT(t, AgentQuestionTemplate::FromText("---\nname: x\ntools: [a]\n---\nhi\n", error) == nullptr);
    TR_ASSERT(t, AgentQuestionTemplate::FromText("---\nname: x\ntools:\n  a: \"\"\n---\nhi\n", error) == nullptr);
    TR_ASSERT(t, AgentQuestionTemplate::FromText("---\nname: [x\n---\nhi\n", error) == nullptr);
    // Always there: root, goatedit
    TR_ASSERT(t, AgentQuestionTemplate::FromText("---\nname: x\n---\n{root} {goatedit}\n", error) != nullptr);
    return kTR_Pass;
}

// Placeholders filled; braces that aren't one stay; '{{' is '{'; a missing value fills to empty
DLL_EXPORT int test_agent_template_fill(ITesting *t) {
    std::string error;
    auto why = AgentQuestionTemplate::FromText(WHY_TEMPLATE, error);
    AgentQuestionTemplate::Values values = {{"file", "src/a.cpp"}, {"line", "12"}, {"tool:chain", "== the chain\n"}};
    auto prompt = why->FillPrompt(values);
    TR_ASSERT(t, prompt == "Line 12 of src/a.cpp:\n== the chain\n\nCode stays: if (x) { return; } - and {line} is literal\n");
    TR_ASSERT(t, why->Describe(values) == "Why is this line here? (src/a.cpp:12)");

    // Args: in order, the missing ones empty; more words than args is not this template
    auto structure = AgentQuestionTemplate::FromText("---\nname: structure\ntitle: Structure\nargs: [base]\n---\nvs {base}.\n", error);
    AgentQuestionTemplate::Values args;
    TR_ASSERT(t, structure->ArgValues({}, args));
    TR_ASSERT(t, args["base"].empty());
    TR_ASSERT(t, structure->FillPrompt(args) == "vs .\n");
    TR_ASSERT(t, structure->Describe(args) == "Structure");
    TR_ASSERT(t, structure->ArgValues({"main"}, args));
    TR_ASSERT(t, args["base"] == "main");
    TR_ASSERT(t, structure->Describe(args) == "Structure (base main)");
    TR_ASSERT(t, !structure->ArgValues({"is", "slow"}, args));
    return kTR_Pass;
}

// A tool's command: split on spaces first, filled after - a value with a space stays one argument, an empty one goes;
// a '.py' runs with python3; only a name, found in the tool folders (the later folder wins)
DLL_EXPORT int test_agent_template_tool(ITesting *t) {
    std::string error;
    auto structure = AgentQuestionTemplate::FromText("---\nname: s\nargs: [base]\nneeds: [file]\ntools:\n"
                                                     "  diff: typediff.py --goatedit {goatedit} {base} --file={file}\n---\n{tool:diff}\n", error);
    TR_ASSERT(t, structure != nullptr);
    AgentQuestionTemplate::Values values = {{"goatedit", "/Apps/My Editor/goatedit"}, {"base", ""}, {"file", "a b.cpp"}};
    auto arguments = structure->ToolArguments(structure->Tools()[0], values);
    TR_ASSERT(t, (arguments == std::vector<std::string>{"--goatedit", "/Apps/My Editor/goatedit", "--file=a b.cpp"}));

    auto command = AgentQuestionTemplate::ToolCommand("/sys/Tools/typediff.py", arguments);
    TR_ASSERT(t, command.program == "python3");
    TR_ASSERT(t, command.arguments.size() == 4);
    TR_ASSERT(t, command.arguments[0] == "/sys/Tools/typediff.py");
    auto binary = AgentQuestionTemplate::ToolCommand("/sys/Tools/scan", {"-x"});
    TR_ASSERT(t, binary.program == "/sys/Tools/scan");
    TR_ASSERT(t, (binary.arguments == std::vector<std::string>{"-x"}));

    auto folder = std::filesystem::temp_directory_path() / "goatedit_agent_template_tool";
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder / "system");
    std::filesystem::create_directories(folder / "user");
    std::ofstream(folder / "system" / "why.py") << "print('system')\n";
    std::ofstream(folder / "system" / "only.py") << "print('system')\n";
    std::ofstream(folder / "user" / "why.py") << "print('user')\n";
    std::vector<std::filesystem::path> folders = {folder / "system", folder / "user"};
    TR_ASSERT(t, AgentQuestionTemplate::FindTool(folders, "why.py") == folder / "user" / "why.py");
    TR_ASSERT(t, AgentQuestionTemplate::FindTool(folders, "only.py") == folder / "system" / "only.py");
    TR_ASSERT(t, AgentQuestionTemplate::FindTool(folders, "none.py").empty());
    TR_ASSERT(t, AgentQuestionTemplate::FindTool(folders, "../system/why.py").empty());
    TR_ASSERT(t, AgentQuestionTemplate::FindTool(folders, "..").empty());
    std::filesystem::remove_all(folder);
    return kTR_Pass;
}

// A tool's output is cut at max_lines, with a line saying how much went - the prompt's cost stays bounded
DLL_EXPORT int test_agent_template_cap(ITesting *t) {
    TR_ASSERT(t, AgentQuestionTemplate::CapOutput("a\nb\n", 2) == "a\nb\n");
    TR_ASSERT(t, AgentQuestionTemplate::CapOutput("a\nb", 2) == "a\nb");
    TR_ASSERT(t, AgentQuestionTemplate::CapOutput("", 2).empty());
    auto cut = AgentQuestionTemplate::CapOutput("a\nb\nc\nd\n", 2);
    TR_ASSERT(t, cut.rfind("a\nb\n[... 2 more lines cut", 0) == 0);
    auto cutNoEnd = AgentQuestionTemplate::CapOutput("a\nb\nc\nd", 3);
    TR_ASSERT(t, cutNoEnd.rfind("a\nb\nc\n[... 1 more lines cut", 0) == 0);
    return kTR_Pass;
}

// System, then user: the user's template of the same name replaces the system's; a file that isn't one is reported
DLL_EXPORT int test_agent_template_folders(ITesting *t) {
    auto folder = std::filesystem::temp_directory_path() / "goatedit_agent_template_folders";
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder / "system");
    std::filesystem::create_directories(folder / "user");
    std::ofstream(folder / "system" / "why.md") << "---\nname: why\ntitle: System\n---\nhi\n";
    std::ofstream(folder / "system" / "other.md") << "---\nname: other\n---\nhi\n";
    std::ofstream(folder / "system" / "notes.txt") << "not a template, not read\n";
    std::ofstream(folder / "user" / "why.md") << "---\nname: why\ntitle: Mine\n---\nhi\n";
    std::ofstream(folder / "user" / "broken.md") << "no front matter\n";

    std::vector<std::string> errors;
    auto templates = AgentQuestionTemplate::LoadAll({folder / "system", folder / "user", folder / "missing"}, errors);
    TR_ASSERT(t, templates.size() == 2);
    auto why = AgentQuestionTemplate::Find(templates, "why");
    TR_ASSERT(t, why != nullptr);
    TR_ASSERT(t, why->Title() == "Mine");
    TR_ASSERT(t, why->Origin() == folder / "user" / "why.md");
    TR_ASSERT(t, AgentQuestionTemplate::Find(templates, "other") != nullptr);
    TR_ASSERT(t, AgentQuestionTemplate::Find(templates, "broken") == nullptr);
    TR_ASSERT(t, errors.size() == 1);
    TR_ASSERT(t, errors[0].find("broken.md") != std::string::npos);
    std::filesystem::remove_all(folder);
    return kTR_Pass;
}

// The shipped templates read, and the tools they name are shipped too
DLL_EXPORT int test_agent_template_shipped(ITesting *t) {
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto questionFolders = assetLoader.ResolveExistingDirectories("Questions", AssetLoaderBase::kLocationType::kSystem);
    auto toolFolders = assetLoader.ResolveExistingDirectories("Tools", AssetLoaderBase::kLocationType::kSystem);
    TR_ASSERT(t, !questionFolders.empty());
    TR_ASSERT(t, !toolFolders.empty());
    std::vector<std::string> errors;
    auto templates = AgentQuestionTemplate::LoadAll(questionFolders, errors);
    TR_ASSERT(t, errors.empty());
    TR_ASSERT(t, AgentQuestionTemplate::Find(templates, "why") != nullptr);
    TR_ASSERT(t, AgentQuestionTemplate::Find(templates, "structure") != nullptr);
    for (auto &questionTemplate : templates) {
        for (auto &tool : questionTemplate->Tools()) {
            TR_ASSERT(t, !AgentQuestionTemplate::FindTool(toolFolders, tool.command.front()).empty());
        }
    }
    return kTR_Pass;
}

// A question from a template says which, and about what - so it can be asked again
DLL_EXPORT int test_agent_question_template_file(ITesting *t) {
    auto question = AgentQuestion::Create("1", "Why is this line here? (src/a.cpp:12)", "/p/.goatedit/agent_qa");
    question->SetPrompt("the prompt");
    auto plain = question->QuestionFileText("Claude Code", "", "now");
    TR_ASSERT(t, plain.find("template:") == std::string::npos);
    question->SetTemplate("why", "src/a.cpp:12");
    auto text = question->QuestionFileText("Claude Code", "", "now");
    TR_ASSERT(t, text.find("template: why\nargs: src/a.cpp:12\n") != std::string::npos);
    TR_ASSERT(t, text.find("# Question\n\nWhy is this line here? (src/a.cpp:12)") != std::string::npos);
    question->SetTemplate("structure", "");
    TR_ASSERT(t, question->QuestionFileText("Claude Code", "", "now").find("args:") == std::string::npos);
    return kTR_Pass;
}

// While it takes a while: the bright dot sweeps there and back, one step per tick, always five wide; the time beside it
DLL_EXPORT int test_agent_busy_indicator(ITesting *t) {
    using namespace std::chrono_literals;
    auto tick = AgentAsk::TICK;
    TR_ASSERT(t, AgentAsk::BusyIndicator(0ms) == U"●•···");
    TR_ASSERT(t, AgentAsk::BusyIndicator(tick) == U"•●•··");
    TR_ASSERT(t, AgentAsk::BusyIndicator(4 * tick) == U"···•●");
    TR_ASSERT(t, AgentAsk::BusyIndicator(5 * tick) == U"··•●•");
    TR_ASSERT(t, AgentAsk::BusyIndicator(8 * tick) == AgentAsk::BusyIndicator(0ms));
    for (int i = 0; i < 20; i++) {
        TR_ASSERT(t, AgentAsk::BusyIndicator(i * tick).size() == 5);
    }
    TR_ASSERT(t, AgentAsk::Elapsed(999ms) == U"0s");
    TR_ASSERT(t, AgentAsk::Elapsed(12s) == U"12s");
    TR_ASSERT(t, AgentAsk::Elapsed(125s) == U"2m 05s");
    TR_ASSERT(t, AgentAsk::Elapsed(-5s) == U"0s");
    return kTR_Pass;
}

// A model's name is one word that can't turn into an option or a second command
DLL_EXPORT int test_agent_model_name(ITesting *t) {
    for (auto name : {"sonnet", "claude-sonnet-5-5", "opus[1m]", "us.anthropic.claude-opus:0"}) {
        TR_ASSERT(t, AgentIntegrations::IsModelName(name));
    }
    for (auto name : {"", "-p", "--model", "a b", "a;b", "a'b", "$(a)", "a\nb"}) {
        TR_ASSERT(t, !AgentIntegrations::IsModelName(name));
    }
    return kTR_Pass;
}

// '.agent model' with 'none': there is no agent to pick one for, and none is picked
DLL_EXPORT int test_agent_model_none(ITesting *t) {
    if (Editor::Instance().GetAgent()->Name() != NoAgent::NAME) {
        return kTR_Pass;
    }
    auto before = Editor::Instance().GetAgentAsk().Model();
    auto lines = Editor::Instance().SetEditorAgentModel("sonnet");
    TR_ASSERT(t, lines.size() == 1);
    TR_ASSERT(t, lines.front().find("none") != std::string::npos);
    TR_ASSERT(t, Editor::Instance().GetAgentAsk().Model() == before);
    return kTR_Pass;
}

// The consent's line about unsaved files: a few by name, the rest counted
DLL_EXPORT int test_agent_unsaved_summary(ITesting *t) {
    TR_ASSERT(t, AgentAsk::UnsavedSummary({}).empty());
    TR_ASSERT(t, AgentAsk::UnsavedSummary({"src/a.cpp"}) == "unsaved: src/a.cpp");
    TR_ASSERT(t, AgentAsk::UnsavedSummary({"a", "b", "c"}) == "unsaved: a, b, c");
    TR_ASSERT(t, AgentAsk::UnsavedSummary({"a", "b", "c", "d", "e"}) == "unsaved: a, b, c and 2 more");
    return kTR_Pass;
}

//
// The agent integration (hitlist #78, docs/agent-integration.md): 'none' says there is nothing, 'claude' gives Claude
// Code's command lines and transcript folder, a name that isn't one makes nothing, and a project config can't pick one.
//
#include <filesystem>
#include <fstream>
#include <testinterface.h>

#include "Core/Agent/AgentIntegrations.h"
#include "Core/Agent/ClaudeAgent.h"
#include "Core/Agent/NoAgent.h"
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
    TR_ASSERT(t, none->QuestionAgentCommand("/p").empty());
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

// Claude Code: 'claude' for the working agent, plan mode (reads, never edits) for the question agent; the prompt names
// the answer file and says it is the only file to change
DLL_EXPORT int test_agent_claude(ITesting *t) {
    auto claude = ClaudeAgent::Create();
    TR_ASSERT(t, claude->DisplayName() == "Claude Code");
    TR_ASSERT(t, (claude->WorkingAgentCommand("/p") == std::vector<std::string>{"claude"}));
    TR_ASSERT(t, (claude->QuestionAgentCommand("/p") == std::vector<std::string>{"claude", "--permission-mode", "plan"}));
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

// The editor has an agent always - 'none' by default (the shipped config)
DLL_EXPORT int test_agent_editor_default(ITesting *t) {
    auto agent = Editor::Instance().GetAgent();
    TR_ASSERT(t, agent != nullptr);
    TR_ASSERT(t, agent->Name() == "none");
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
    auto banner = AgentIntegrations::EditorAgentBanner(*claude, claude->QuestionAgentCommand("/p"));
    TR_ASSERT(t, banner.find("Editor Agent") != std::string::npos);
    TR_ASSERT(t, banner.find("GoatEdit") != std::string::npos);
    TR_ASSERT(t, banner.find("Claude Code") != std::string::npos);
    TR_ASSERT(t, banner.find("claude --permission-mode plan") != std::string::npos);
    TR_ASSERT(t, banner.find("\r\n") != std::string::npos);
    return kTR_Pass;
}

// With 'none' there is nothing to start - and nothing is
DLL_EXPORT int test_agent_start_none(ITesting *t) {
    auto message = Editor::Instance().StartEditorAgent();
    TR_ASSERT(t, message.find("none") != std::string::npos);
    return kTR_Pass;
}

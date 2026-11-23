//
// The editor's side of an AI agent (hitlist #78, docs/agent-integration.md): what it needs to know about the agent the
// user has - how to start it in a tab, where it keeps its transcripts, how a question is put to it. One implementation
// per agent; 'none' (NoAgent) is the default and the editor as it was - nothing is started, nothing is sent.
//
// Picked by 'main.ai_integration' (none | claude) in the user config. A project config can't set it: agent calls cost
// money and run on the user's account, so a folder never switches one on (ProjectConfigPolicy). Whether a project is
// Integrated at all is the project profile's (docs/project-profile.md) - not built yet; this is which agent, if any.
//
// Plain data out: command lines, paths, text. The editor starts the command in a terminal tab (the PTY channel, §4) -
// nothing here runs a process.
//

#ifndef GEDIT_AGENTINTEGRATION_H
#define GEDIT_AGENTINTEGRATION_H

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gedit {
    class AgentIntegration {
    public:
        using Ref = std::shared_ptr<AgentIntegration>;
    public:
        AgentIntegration() = default;
        virtual ~AgentIntegration() = default;

        // As main.ai_integration names it: 'none', 'claude'
        virtual std::string Name() const = 0;
        // What the user calls it ('Claude Code'); empty for none
        virtual std::string DisplayName() const = 0;
        // Its command is installed - advisory: a tab starts it through the user's shell, whose PATH may hold more
        virtual bool IsAvailable() const = 0;
        // One line for the user: what it is and whether it can be used
        virtual std::string Status() const = 0;

        // The command line for the user's own agent in a panel tab, in 'root'. Empty: there is none.
        virtual std::vector<std::string> WorkingAgentCommand(const std::filesystem::path &root) const = 0;
        // The question agent (§6.1): its own process and conversation, started read-only. Empty: there is none.
        virtual std::vector<std::string> QuestionAgentCommand(const std::filesystem::path &root) const = 0;
        // Where the agent keeps this project's transcripts - a session's id is its file's name (§5.2). nullopt: unknown.
        virtual std::optional<std::filesystem::path> TranscriptDir(const std::filesystem::path &root) const = 0;
        // A question as it is typed into the agent (the PTY channel, §4): the answer comes back as a file, not as
        // screen text. Empty: no agent to ask.
        virtual std::string PromptWithAnswerFile(const std::string &question, const std::filesystem::path &answerFile) const = 0;
    };
}

#endif //GEDIT_AGENTINTEGRATION_H

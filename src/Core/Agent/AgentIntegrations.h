//
// The agents there are (hitlist #78): 'none' and 'claude' - made by name, as main.ai_integration says.
//

#ifndef GEDIT_AGENTINTEGRATIONS_H
#define GEDIT_AGENTINTEGRATIONS_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "AgentIntegration.h"
#include "Core/Config/ConfigNode.h"

namespace gedit {
    class AgentIntegrations {
    public:
        // The agent by its name; nullptr for a name there is none of (the caller says so and uses 'none')
        static AgentIntegration::Ref Create(const std::string &name);
        static std::vector<std::string> Names();

        // An executable by name: on $PATH, else in the places an installer puts one - a Mac app started from the Dock
        // doesn't get the shell's PATH ('~/.local/bin', '~/.claude/local', '/opt/homebrew/bin', '/usr/local/bin')
        // (the search itself: Platform::FindExecutable)
        static std::optional<std::filesystem::path> FindExecutable(const std::string &name);

        // --- The Editor Agent's tab (agent-integration.md §6.2) ---
        // A command as one shell line - words with anything but [A-Za-z0-9_./=:+-] in single quotes
        static std::string ShellCommandLine(const std::vector<std::string> &command);
        // The terminal settings it runs with: the user's 'terminal' section (shell, colours), its bootstrap replaced by
        // the agent's command; never saved, never the promote target, never the output console. A copy - the
        // section itself is left alone.
        static ConfigNode EditorAgentSettings(const ConfigNode &terminalSettings, const std::vector<std::string> &command);
        // The line at the top of the tab: whose agent this is, what it runs, how it ends - most important first (a
        // narrow panel cuts the end)
        static std::string EditorAgentBanner(const AgentIntegration &agent, const std::vector<std::string> &command);
        // Something that can be a model's name ('sonnet', 'claude-sonnet-5-5', 'opus[1m]') - one word, not an option
        static bool IsModelName(const std::string &name);
        // A random (version 4) UUID - the id the editor gives the Editor Agent's conversation (§5.2, §6.3)
        static std::string NewSessionId();
    };
}

#endif //GEDIT_AGENTINTEGRATIONS_H

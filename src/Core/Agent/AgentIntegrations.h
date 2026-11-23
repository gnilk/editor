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

namespace gedit {
    class AgentIntegrations {
    public:
        // The agent by its name; nullptr for a name there is none of (the caller says so and uses 'none')
        static AgentIntegration::Ref Create(const std::string &name);
        static std::vector<std::string> Names();

        // An executable by name: on $PATH, else in the places an installer puts one - a Mac app started from the Dock
        // doesn't get the shell's PATH ('~/.local/bin', '~/.claude/local', '/opt/homebrew/bin', '/usr/local/bin')
        static std::optional<std::filesystem::path> FindExecutable(const std::string &name);
        // The same, with the search spelled out (tests)
        static std::optional<std::filesystem::path> FindExecutable(const std::string &name, const std::string &pathVariable,
                                                                   const std::vector<std::filesystem::path> &moreDirs);
    };
}

#endif //GEDIT_AGENTINTEGRATIONS_H

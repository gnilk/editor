//
// See AgentIntegrations.h
//

#include <cctype>
#include <cstdlib>
#include <unistd.h>

#include "AgentIntegrations.h"
#include "ClaudeAgent.h"
#include "NoAgent.h"

using namespace gedit;

AgentIntegration::Ref AgentIntegrations::Create(const std::string &name) {
    if (name == NoAgent::NAME) {
        return NoAgent::Create();
    }
    if (name == ClaudeAgent::NAME) {
        return ClaudeAgent::Create();
    }
    return nullptr;
}

std::vector<std::string> AgentIntegrations::Names() {
    return {NoAgent::NAME, ClaudeAgent::NAME};
}

std::optional<std::filesystem::path> AgentIntegrations::FindExecutable(const std::string &name) {
    auto pathVariable = std::getenv("PATH");
    std::vector<std::filesystem::path> moreDirs;
    if (auto home = std::getenv("HOME"); (home != nullptr) && (*home != '\0')) {
        moreDirs.push_back(std::filesystem::path(home) / ".local" / "bin");
        moreDirs.push_back(std::filesystem::path(home) / ".claude" / "local");
    }
    moreDirs.emplace_back("/opt/homebrew/bin");
    moreDirs.emplace_back("/usr/local/bin");
    return FindExecutable(name, (pathVariable != nullptr) ? pathVariable : "", moreDirs);
}

std::optional<std::filesystem::path> AgentIntegrations::FindExecutable(const std::string &name, const std::string &pathVariable,
                                                                       const std::vector<std::filesystem::path> &moreDirs) {
    if (name.empty() || (name.find('/') != std::string::npos)) {
        return {};
    }
    std::vector<std::filesystem::path> dirs;
    size_t start = 0;
    while (start <= pathVariable.size()) {
        auto end = pathVariable.find(':', start);
        if (end == std::string::npos) {
            end = pathVariable.size();
        }
        if (end > start) {
            dirs.emplace_back(pathVariable.substr(start, end - start));
        }
        start = end + 1;
    }
    dirs.insert(dirs.end(), moreDirs.begin(), moreDirs.end());
    for (auto &dir : dirs) {
        auto candidate = dir / name;
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec) && (access(candidate.c_str(), X_OK) == 0)) {
            return candidate;
        }
    }
    return {};
}

std::string AgentIntegrations::ShellCommandLine(const std::vector<std::string> &command) {
    std::string line;
    for (auto &word : command) {
        bool isPlain = !word.empty();
        for (auto ch : word) {
            if (!std::isalnum((unsigned char)ch) && (std::string("_./=:+-").find(ch) == std::string::npos)) {
                isPlain = false;
                break;
            }
        }
        std::string quoted = word;
        if (!isPlain) {
            quoted = "'";
            for (auto ch : word) {
                quoted += (ch == '\'') ? std::string("'\\''") : std::string(1, ch);
            }
            quoted += "'";
        }
        line += (line.empty() ? "" : " ") + quoted;
    }
    return line;
}

ConfigNode AgentIntegrations::EditorAgentSettings(const ConfigNode &terminalSettings, const std::vector<std::string> &command) {
    auto node = terminalSettings.GetDataNode().IsMap() ? YAML::Clone(terminalSettings.GetDataNode()) : YAML::Node(YAML::NodeType::Map);
    ConfigNode settings(node);
    settings.SetValue("bootstrap", std::vector<std::string>{ShellCommandLine(command)});
    settings.SetBool("persist_scrollback", false);
    settings.SetBool("promote_target", false);
    settings.SetBool("output_console", false);
    return settings;
}

std::string AgentIntegrations::EditorAgentBanner(const AgentIntegration &agent, const std::vector<std::string> &command) {
    // Two short lines - the panel is low, and they must still be there when the agent has started
    std::string banner;
    banner += "\x1b[1;35mEditor Agent\x1b[0m: started by GoatEdit for its own questions - not your agent.\r\n";
    banner += agent.DisplayName() + ", read-only: " + ShellCommandLine(command) + " - closing this tab stops it.\r\n";
    return banner;
}

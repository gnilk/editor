//
// See AgentIntegrations.h
//

#include <cctype>
#include <cstdlib>
#include <random>

#include "AgentIntegrations.h"
#include "ClaudeAgent.h"
#include "NoAgent.h"
#include "Core/UserDirs.h"
#include "Core/Platform.h"

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
    std::vector<std::filesystem::path> moreDirs;
    if (auto &home = UserDirs::Home(); !home.empty()) {
        moreDirs.push_back(home / ".local" / "bin");
        moreDirs.push_back(home / ".claude" / "local");
    }
    moreDirs.emplace_back("/opt/homebrew/bin");
    moreDirs.emplace_back("/usr/local/bin");
    return Platform::FindExecutable(name, Platform::GetEnvOr("PATH", ""), moreDirs);
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
    // Its conversation ended with its shell - a new agent is a new conversation ('.agent start')
    settings.SetBool("enter_restarts_shell", false);
    settings.SetStr("shell_ended_hint", "'.agent start' starts a new Editor Agent");
    return settings;
}

std::string AgentIntegrations::EditorAgentBanner(const AgentIntegration &agent, const std::vector<std::string> &command) {
    return "GoatEdit's own agent, for its questions - not yours | " + agent.DisplayName() + ", read-only: " +
           ShellCommandLine(command) + " | closing this tab stops it";
}

bool AgentIntegrations::IsModelName(const std::string &name) {
    if (name.empty() || (name.front() == '-')) {
        return false;
    }
    for (auto ch : name) {
        if (!std::isalnum((unsigned char)ch) && (std::string("_./:@[]-").find(ch) == std::string::npos)) {
            return false;
        }
    }
    return true;
}

std::string AgentIntegrations::NewSessionId() {
    std::random_device device;
    std::mt19937_64 generator(((uint64_t)device() << 32) ^ device());
    std::uniform_int_distribution<int> nibble(0, 15);
    static const char *HEX = "0123456789abcdef";
    std::string id;
    for (int i = 0; i < 32; i++) {
        auto value = nibble(generator);
        if (i == 12) {
            value = 4;                      // version 4
        } else if (i == 16) {
            value = 8 | (value & 3);        // variant 10xx
        }
        if ((i == 8) || (i == 12) || (i == 16) || (i == 20)) {
            id += '-';
        }
        id += HEX[value];
    }
    return id;
}

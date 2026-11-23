//
// See ClaudeAgent.h
//

#include <cctype>
#include <cstdlib>

#include "AgentIntegrations.h"
#include "ClaudeAgent.h"

using namespace gedit;

bool ClaudeAgent::IsAvailable() const {
    return AgentIntegrations::FindExecutable(COMMAND).has_value();
}

std::string ClaudeAgent::Status() const {
    auto path = AgentIntegrations::FindExecutable(COMMAND);
    if (!path.has_value()) {
        return "claude - Claude Code; 'claude' not found on PATH or in the usual places (a tab's shell may still find it)";
    }
    return "claude - Claude Code (" + path->string() + ")";
}

std::vector<std::string> ClaudeAgent::WorkingAgentCommand(const std::filesystem::path &) const {
    return {COMMAND};
}

std::vector<std::string> ClaudeAgent::QuestionAgentCommand(const std::filesystem::path &) const {
    return {COMMAND, "--permission-mode", "plan"};
}

std::optional<std::filesystem::path> ClaudeAgent::TranscriptDir(const std::filesystem::path &root) const {
    auto configDir = ConfigDir();
    if (configDir.empty() || root.empty()) {
        return {};
    }
    return configDir / "projects" / ProjectFolderName(root);
}

std::string ClaudeAgent::PromptWithAnswerFile(const std::string &question, const std::filesystem::path &answerFile) const {
    return question + "\n\nWrite your answer, as Markdown, to the file " + answerFile.string() +
           " - create or replace it - and change no other file.";
}

std::string ClaudeAgent::ProjectFolderName(const std::filesystem::path &root) {
    auto name = root.lexically_normal().string();
    while ((name.size() > 1) && (name.back() == '/')) {
        name.pop_back();
    }
    for (auto &ch : name) {
        if (!std::isalnum((unsigned char)ch)) {
            ch = '-';
        }
    }
    return name;
}

std::filesystem::path ClaudeAgent::ConfigDir() {
    if (auto configDir = std::getenv("CLAUDE_CONFIG_DIR"); (configDir != nullptr) && (*configDir != '\0')) {
        return configDir;
    }
    if (auto home = std::getenv("HOME"); (home != nullptr) && (*home != '\0')) {
        return std::filesystem::path(home) / ".claude";
    }
    return {};
}

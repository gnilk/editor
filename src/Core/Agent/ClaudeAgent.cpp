//
// See ClaudeAgent.h
//

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>

#include <nlohmann/json.hpp>

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

std::vector<std::string> ClaudeAgent::QuestionAgentCommand(const std::filesystem::path &root, const std::string &sessionId,
                                                           const std::string &model) const {
    // '//' starts an absolute path in Claude Code's permission rules - the rule holds whatever the tab's directory is
    auto answers = (root.lexically_normal() / ".goatedit" / "agent_qa" / "a_*").string();
    std::vector<std::string> command = {COMMAND, "--permission-mode", "dontAsk",
                                        "--allowedTools", "Read Grep Glob Edit(/" + answers + ")"};
    if (!sessionId.empty()) {
        command.push_back("--session-id");
        command.push_back(sessionId);
    }
    if (!model.empty()) {
        command.push_back("--model");
        command.push_back(model);
    }
    return command;
}

std::optional<std::filesystem::path> ClaudeAgent::TranscriptFile(const std::filesystem::path &root, const std::string &sessionId) const {
    auto dir = TranscriptDir(root);
    if (!dir.has_value() || sessionId.empty()) {
        return {};
    }
    auto file = dir.value() / (sessionId + ".jsonl");
    std::error_code ec;
    if (std::filesystem::exists(file, ec)) {
        return file;
    }
    // Claude Code may name the folder after another spelling of the path ('/var/...' for '/private/var/...', a symlink)
    // - a session id is unique, so look for it in every project folder
    for (auto &entry : std::filesystem::directory_iterator(dir->parent_path(), ec)) {
        auto candidate = entry.path() / (sessionId + ".jsonl");
        if (std::filesystem::exists(candidate, ec)) {
            return candidate;
        }
    }
    return file;
}

std::optional<AgentIntegration::Usage> ClaudeAgent::UsageSince(const std::filesystem::path &transcript, uintmax_t fromByte) const {
    std::ifstream in(transcript, std::ios::binary);
    if (!in) {
        return {};
    }
    in.seekg((std::streamoff)fromByte);
    // Per message id, the last usage seen - then summed
    std::map<std::string, nlohmann::json> usageById;
    std::vector<std::string> order;
    std::string model;
    std::string line;
    while (std::getline(in, line)) {
        auto entry = nlohmann::json::parse(line, nullptr, false);
        if (entry.is_discarded() || !entry.is_object() || (entry.value("type", "") != "assistant")) {
            continue;
        }
        auto message = entry.find("message");
        if ((message == entry.end()) || !message->is_object() || !message->contains("usage")) {
            continue;
        }
        auto id = message->value("id", std::to_string(order.size()));
        if (usageById.find(id) == usageById.end()) {
            order.push_back(id);
        }
        usageById[id] = (*message)["usage"];
        model = message->value("model", model);
    }
    Usage usage;
    usage.model = model;
    for (auto &id : order) {
        auto &one = usageById[id];
        usage.numCalls += 1;
        usage.inputTokens += one.value("input_tokens", (uint64_t)0);
        usage.cacheWriteTokens += one.value("cache_creation_input_tokens", (uint64_t)0);
        usage.cacheReadTokens += one.value("cache_read_input_tokens", (uint64_t)0);
        usage.outputTokens += one.value("output_tokens", (uint64_t)0);
    }
    return usage;
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
           " - create or replace it - and change no other file. Don't read the other files in its folder: earlier "
           "questions and answers, stale or wrong by now. Shell commands here are limited to ones that only read; one "
           "per call, and when one is refused use Read and Grep instead of another form of it.";
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

std::string ClaudeAgent::StartBlocker(const std::filesystem::path &root) const {
    if (IsTrusted(root, ConfigFile())) {
        return {};
    }
    return "Claude Code isn't trusted in " + root.string() + " yet - run 'claude' there once and accept, then ask again "
           "(its trust dialog would be answered by the editor's Enter)";
}

bool ClaudeAgent::IsTrusted(const std::filesystem::path &root, const std::filesystem::path &configFile) {
    std::ifstream in(configFile, std::ios::binary);
    if (!in) {
        return false;
    }
    auto config = nlohmann::json::parse(in, nullptr, false);
    if (config.is_discarded() || !config.is_object() || !config.contains("projects") || !config["projects"].is_object()) {
        return false;
    }
    auto &projects = config["projects"];
    auto dir = root.lexically_normal();
    while (!dir.empty()) {
        auto key = dir.string();
        while ((key.size() > 1) && (key.back() == '/')) {
            key.pop_back();
        }
        auto project = projects.find(key);
        if ((project != projects.end()) && project->is_object() && project->value("hasTrustDialogAccepted", false)) {
            return true;
        }
        if (dir == dir.parent_path()) {
            break;
        }
        dir = dir.parent_path();
    }
    return false;
}

std::filesystem::path ClaudeAgent::ConfigFile() {
    if (auto configDir = std::getenv("CLAUDE_CONFIG_DIR"); (configDir != nullptr) && (*configDir != '\0')) {
        return std::filesystem::path(configDir) / ".claude.json";
    }
    if (auto home = std::getenv("HOME"); (home != nullptr) && (*home != '\0')) {
        return std::filesystem::path(home) / ".claude.json";
    }
    return {};
}

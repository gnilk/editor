//
// See AgentIntegrations.h
//

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

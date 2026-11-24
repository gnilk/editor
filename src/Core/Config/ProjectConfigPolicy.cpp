//
// See ProjectConfigPolicy.h
//

#include <algorithm>
#include <utility>
#include "ProjectConfigPolicy.h"

using namespace gedit;

// Keys that pick code to load or a place to write, by section
static const std::vector<std::pair<std::string, std::string>> deniedKeys = {
    { "main",     "bootstrap_scripts" },
    { "main",     "plugin_directory" },
    { "main",     "ai_integration" },       // an agent runs on the user's account - a folder never switches one on
    { "main",     "ai_settle_ms" },         // when the editor types into the agent - the user's, with the agent
    { "logging",  "logfile" },
};

// Keys that run a command - denied in EVERY section, at any depth: a configured tab hands any section to a terminal
// ('ai_agent: { inherit: terminal, bootstrap: [claude] }'), so 'terminal' is not the only place one can sit
static const std::vector<std::string> commandKeys = { "shell", "init", "bootstrap" };

static bool IsCommandKey(const std::string &key) {
    return std::find(commandKeys.begin(), commandKeys.end(), key) != commandKeys.end();
}

static void StripCommandKeys(YAML::Node node, const std::string &path, std::vector<std::string> &removed);

bool ProjectConfigPolicy::IsDenied(const std::vector<std::string> &keyPath) {
    if (keyPath.empty()) {
        return false;
    }
    // A whole section would replace the section - its command keys included
    if (keyPath.size() == 1) {
        return true;
    }
    for (size_t i = 1; i < keyPath.size(); i++) {
        if (IsCommandKey(keyPath[i])) {
            return true;
        }
    }
    for (auto &[section, key] : deniedKeys) {
        if ((keyPath[0] == section) && (keyPath[1] == key)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> ProjectConfigPolicy::StripDeniedKeys(ConfigNode &projectConfig) {
    std::vector<std::string> removed;
    std::vector<std::string> sections;
    for (auto const &entry : projectConfig.GetDataNode()) {
        sections.push_back(entry.first.Scalar());
    }
    for (auto &section : sections) {
        auto sectionNode = projectConfig.GetNode(section);
        // A section given as a scalar or a list would replace the whole section on merge
        if (!sectionNode.GetDataNode().IsMap()) {
            projectConfig.RemoveKey(section);
            removed.push_back(section);
            continue;
        }
        for (auto &[deniedSection, key] : deniedKeys) {
            if ((deniedSection == section) && sectionNode.RemoveKey(key)) {
                removed.push_back(section + "." + key);
            }
        }
        StripCommandKeys(sectionNode.GetDataNode(), section, removed);
    }
    return removed;
}

// Below a section: maps lose their command keys, lists are walked item by item ('panel.tabs: [ {...} ]')
static void StripCommandKeys(YAML::Node node, const std::string &path, std::vector<std::string> &removed) {
    if (node.IsSequence()) {
        for (size_t i = 0; i < node.size(); i++) {
            StripCommandKeys(node[i], path + "." + std::to_string(i), removed);
        }
        return;
    }
    if (!node.IsMap()) {
        return;
    }
    std::vector<std::string> keys;
    for (auto const &entry : node) {
        keys.push_back(entry.first.Scalar());
    }
    for (auto &key : keys) {
        if (IsCommandKey(key)) {
            node.remove(key);
            removed.push_back(path + "." + key);
        } else {
            StripCommandKeys(node[key], path + "." + key, removed);
        }
    }
}

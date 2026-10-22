//
// See ProjectConfigPolicy.h
//

#include <utility>
#include "ProjectConfigPolicy.h"

using namespace gedit;

static const std::vector<std::pair<std::string, std::string>> deniedKeys = {
    { "main",     "bootstrap_scripts" },
    { "main",     "plugin_directory" },
    { "terminal", "shell" },
    { "terminal", "init" },
    { "terminal", "bootstrap" },
    { "logging",  "logfile" },
};

bool ProjectConfigPolicy::IsDenied(const std::vector<std::string> &keyPath) {
    if (keyPath.empty()) {
        return false;
    }
    for (auto &[section, key] : deniedKeys) {
        if (keyPath[0] != section) {
            continue;
        }
        // The section itself (would replace it whole) or anything at/below the denied key
        if ((keyPath.size() == 1) || (keyPath[1] == key)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> ProjectConfigPolicy::StripDeniedKeys(ConfigNode &projectConfig) {
    std::vector<std::string> removed;
    for (auto &[section, key] : deniedKeys) {
        if (!projectConfig.HasKey(section)) {
            continue;
        }
        auto sectionNode = projectConfig.GetNode(section);
        if (!sectionNode.GetDataNode().IsMap()) {
            projectConfig.RemoveKey(section);
            removed.push_back(section);
            continue;
        }
        if (sectionNode.RemoveKey(key)) {
            removed.push_back(section + "." + key);
        }
    }
    return removed;
}

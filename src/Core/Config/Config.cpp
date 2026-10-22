//
// Created by gnilk on 21.01.23.
//
#include <logger.h>
#include <cstdio>
#include <fstream>

#include <yaml-cpp/yaml.h>
#include "logger.h"

#include "Config.h"

//
// The config is using YAML - because I wanted to test it - and because it feels like the right tool for this kind of
// thing...
// Have to say YAML feels like a marriage between JSON and INI-files, with just enough rope to hang yourself with
// the indentation craze..  by hey - I just love invisible control structures...  hello Python...
//
using namespace gedit;

Config::Config() : ConfigNode() {

}

Config &Config::Instance() {
    static Config glbConfig;
    if (glbConfig.logger == nullptr) {
        glbConfig.logger = gnilk::Logger::GetLogger("Config");
    }
    return glbConfig;
}

bool Config::LoadSystemConfig(const std::string &filename) {
    if (!ConfigNode::LoadConfig(filename, AssetLoaderBase::kLocationType::kSystem)) {
        return false;
    }
    SetDefaultsIfMissing();
    if (!dataNode.IsDefined()) {
        return false;
    }
    if (!HasKey("main")) {
        logger->Error("ERR: Configuration has no 'main' section");
        return false;
    }

    return true;
}

void Config::TryMergeUserConfig(const std::string &filename, bool replace) {
    ConfigNode userConfig;
    if (!userConfig.LoadConfig(filename, AssetLoaderBase::kLocationType::kUser)) {
        logger->Warn("No user config found, skipping");
        return;
    }
    logger->Debug("User config loaded - now merging");
    MergeNode(userConfig);
}

void Config::TryMergeProjectConfig(const std::string &filename) {
    ConfigNode projectConfig;
    if (!projectConfig.LoadConfig(filename, AssetLoaderBase::kLocationType::kProject)) {
        logger->Debug("No project config found, skipping");
        return;
    }
    // A non-map document (scalar/sequence) would replace the whole config on merge - refuse it.
    if (!projectConfig.GetDataNode().IsMap()) {
        logger->Warn("Project config is not a map, ignored");
        return;
    }
    for (auto &key : StripProjectUnsafeKeys(projectConfig)) {
        logger->Warn("Project config: '%s' is not allowed in a project config, ignored", key.c_str());
    }
    logger->Debug("Project config loaded - now merging");
    MergeNode(projectConfig);
}

// Keys that start processes, load code or choose where files get written. A cloned repository can carry
// a '.goatedit/config.yml', so opening a folder must never be enough to run someone else's commands.
// These stay system/user-only.
static const std::vector<std::pair<std::string, std::string>> projectDeniedKeys = {
    { "main",     "bootstrap_scripts" },
    { "main",     "plugin_directory" },
    { "terminal", "shell" },
    { "terminal", "init" },
    { "terminal", "bootstrap" },
    { "logging",  "logfile" },
};

std::vector<std::string> Config::StripProjectUnsafeKeys(ConfigNode &projectConfig) {
    std::vector<std::string> removed;
    for (auto &[section, key] : projectDeniedKeys) {
        if (!projectConfig.HasKey(section)) {
            continue;
        }
        // A section given as a non-map would replace the whole section on merge (denied keys included).
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


extern std::string glbDefaultConfig;
void Config::SetDefaultsIfMissing() {
    return;
    // Check root..
    auto defaultConfNode = YAML::Load(glbDefaultConfig);
    if (!HasKey("commandmode")) {
        dataNode.push_back(defaultConfNode["commandmode"]);
    }
}

std::string glbDefaultConfig=""\
"main:"\
"  backend: ncurses"\
"  keymap: default"\
"macos:"\
"  allow_kbd_hook: yes"\
"ncurses:"\
"editor:"\
"commandmode:"\
"  prompt: \"ed>\""\
"terminal:"\
"  shell: /bin/zsh"\
"  init: -is"\
"  bootstrap:"\
"    - SET XYZ=4"\
"    - export path=%path%"\
"    - ls -laF"\
"theme:"\
"  sublime_colorfile: \"tests/colors.sublime.json\""\
"languages:"\
"  default:"\
"    indent: 4"\
"    tabsize: 4"\
"    insert_spaces: yes"\
"  cpp:"\
"    insert_closing_brace: yes"\
"    auto_indent: yes"\
"    continue_comment: yes";


//
// Created by gnilk on 21.01.23.
//
#include <logger.h>
#include <cstdio>
#include <fstream>

#include <yaml-cpp/yaml.h>
#include "logger.h"

#include "Config.h"

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
    auto themeFile = (*this)["main"].GetStr("theme", "default.theme.yml");
    if (!LoadTheme(themeFile)) {
        // output some error here
        logger->Error("ERR: Missing theme, tried: '%s'",themeFile.c_str());
        return false;
    }

    return true;
}

bool Config::MergeUserConfig(const std::string &filename, bool replace) {
    ConfigNode userConfig;
    if (!userConfig.LoadConfig(filename, AssetLoaderBase::kLocationType::kUser)) {
        logger->Error("User config '%s' not found", filename.c_str());
        return false;
    }
    logger->Debug("User config loaded - now merging\n");

    MergeNode(userConfig);

    return true;
}


bool Config::LoadTheme(const std::string &themeFile) {
    theme = Theme::Create();
    if (theme == nullptr) {
        return false;
    }
    return theme->Load(themeFile);
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
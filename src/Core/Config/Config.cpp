//
// Created by gnilk on 21.01.23.
//
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
    return glbConfig;
}

bool Config::LoadConfig(const std::string &filename) {
    if (!ConfigNode::LoadConfig(filename)) {
        return false;
    }
    SetDefaultsIfMissing();
    // FIXME: add defaults to missing stuff
    if (!dataNode.IsDefined()) {
        return false;
    }
    if (!HasKey("main")) {
        printf("ERR: Configuration has no 'main' section");
        // FIXME: Create defaults here..
        return false;
    }
    auto themeFile = (*this)["main"].GetStr("theme", "default.theme.yml");
    if (!LoadTheme(themeFile)) {
        // output some error here
        printf("ERR: Missing theme, tried: '%s'",themeFile.c_str());
        return false;
    }

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
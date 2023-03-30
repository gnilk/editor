//
// Created by gnilk on 21.01.23.
//

#include "Config.h"
#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>

#include "logger.h"

#include "Core/Sublime/SublimeConfigScriptEngine.h"
#include "Core/Sublime/SublimeConfigColorScript.h"


#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>


// for sublime color script handling
using json = nlohmann::json;


Config::Config() : ConfigNode() {

}

Config &Config::Instance() {
    static Config glbConfig;
    return glbConfig;
}

void Config::RegisterLanguage(const std::string &extension, LanguageBase *languageBase) {
    extToLanguages[extension] = languageBase;
}

LanguageBase *Config::GetLanguageForFilename(const std::string &extension) {
    // FIXME: We have this one (for now)
    return extToLanguages[".cpp"];
}


bool Config::LoadConfig(const std::string &filename) {
    dataNode = YAML::LoadFile(filename);
    SetDefaultsIfMissing();
    // FIXME: add defaults to missing stuff
    if (!dataNode.IsDefined()) {
        return false;
    }
    // Load theme
    if (HasKey("theme")) {
        auto themeKey = (*this)["theme"];
        if (themeKey.HasKey("sublime_colorfile")) {
            auto filename = themeKey.GetStr("sublime_colorfile");
            if (!LoadSublimeColorFile(filename)) {
                printf("ERR: Unable to load sublime color file (%s)\n", filename.c_str());
            }
        }  else {
            printf("No sublime color file in theme section\n");
        }
    } else {
        printf("Configuration has no theme...");
    }

    return true;
}

bool Config::LoadSublimeColorFile(const std::string &filename) {

    // Only enough here - or do we need this one out side???
    SublimeConfigColorScript scriptEngine;
    scriptEngine.RegisterBuiltIn();

    auto logger = gnilk::Logger::GetLogger("Config");
    logger->Debug("Loading Sublime Color file: %s\n", filename.c_str());

    std::ifstream f(filename);

    json data = json::parse(f);
    auto variables = data["variables"];
    for (auto &col : variables.items()) {
        if (col.value().is_string()) {
            auto value = col.value().get<std::string>();

            //printf("  %s:%s\n", col.key().c_str(), value.c_str());

            auto [ok, color] = scriptEngine.ExecuteColorScript(value);

            scriptEngine.AddVarFromValue<ColorRGBA>(col.key(), SublimeConfigScriptEngine::kColor, color);
        }
    }

    auto globals = data["globals"];
    for(auto &col : globals.items()) {
        if (col.value().is_string()) {
            auto value = col.value().get<std::string>();

            auto [ok, scriptValue] = scriptEngine.ExecuteScript(value);
            if (ok && scriptValue.IsColor()) {
                colorConfig.SetColor(col.key(), scriptValue.Color());
            } else {
                logger->Error("  Value for '%s' is not color, constants not supported - skipping\n", col.key().c_str());
            }
        }
    }




//    printf("Testing script engine\n");
//    auto colValue = scriptEngine.GetVariable("blue3").Color();
//    printf("col: %f, %f, %f", colValue.R(), colValue.G(), colValue.B());
//
    return true;
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
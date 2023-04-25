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
using namespace gedit;

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
        if (themeKey.HasKey("colorfile")) {
            auto filename = themeKey.GetStr("colorfile");
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

    ParseVariablesInScript<json, SublimeConfigColorScript>(data["variables"], scriptEngine);
    logger->Debug("Mapping color variables to sections");
    for (auto &colorSection : data["colors"].items()) {
        if (!colorSection.value().is_object()) {
            logger->Debug("Colors should only contain objects!");
            continue;
        }
        logger->Debug("- %s", colorSection.key().c_str());
        SetNamedColorsFromScript<json, SublimeConfigColorScript>(colorConfig[colorSection.key()], colorSection.value(), scriptEngine);

    }
    return true;
}

template<typename T, typename E>
void Config::ParseVariablesInScript(const T &variables, E &scriptEngine) {
    for (auto &col : variables.items()) {
        if (col.value().is_string()) {
            // Ok, this is a bit weird (at least it was for me) but the deal goes like this
            // due to 'get<std::string>' (which is a template specialization) and we are calling that on a templated resuls (col.value())
            // we need to tell the compiler that this is really a template specialization otherwise it would treat it as '>' '<' (larger than/smaller than)
            // This is actually outlined in the standard (found it in draft through some site) https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4296.pdf
            auto value = col.value().template get<std::string>();

            auto [ok, color] = scriptEngine.ExecuteColorScript(value);

            scriptEngine.template AddVarFromValue<ColorRGBA>(col.key(), SublimeConfigScriptEngine::kColor, color);
        }
    }
}

template<typename T, typename E>
void Config::SetNamedColorsFromScript(NamedColorConfig &dstColorConfig, const T &globals, E &scriptEngine) {
    for(auto &col : globals.items()) {
        if (col.value().is_string()) {
            auto value = col.value().template get<std::string>();

            auto [ok, scriptValue] = scriptEngine.ExecuteScript(value);
            if (ok && scriptValue.IsColor()) {
                dstColorConfig.SetColor(col.key(), scriptValue.Color());
            } else {
                auto logger = gnilk::Logger::GetLogger("Config");
                logger->Error("  Value for '%s' is not color, constants not supported - skipping\n", col.key().c_str());
            }
        }
    }
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
//
// Created by gnilk on 22.07.23.
//

// TO-DO: Consider supporting Helix TOML files..

#include <fstream>

#include <nlohmann/json.hpp>

#include "Core/Sublime/SublimeConfigColorScript.h"
#include "Theme.h"

#include "logger.h"

using json = nlohmann::json;
using namespace gedit;

Theme::Ref Theme::Create() {
    auto instance = std::make_shared<Theme>();
    if (!instance->Initialize()) {
        return nullptr;
    }
    return instance;
}

bool Theme::Initialize() {
    logger = gnilk::Logger::GetLogger("Theme");
    return true;
}

bool Theme::Reload() {
    if  (filename.empty()) {
        logger->Error("Theme must first be loaded");
        return false;
    }
    // Clear colors..
    colorConfig.clear();
    return Load(filename);
}


bool Theme::Load(const std::string &pathname) {
    if (!ConfigNode::LoadConfig(pathname)) {
        return false;
    }

    name = GetStr("name","noname");

    if (!HasKey("colorfile")) {
        logger->Warning("No color file defined for theme '%s' (theme file: %s)", name.c_str(), pathname.c_str());
        return false;
    }

    auto colorFile = GetStr("colorfile", "colors.json");
    auto colorsOk = LoadSublimeColorFile(colorFile);
    if (!colorsOk) {
        logger->Error("Failed to load colors from: '%s'", colorFile.c_str());
        return false;
    }

    logger->Debug("Ok, theme '%s' loaded from file: %s", name.c_str(), pathname.c_str());

    filename = pathname;
    return true;
}

bool Theme::LoadSublimeColorFile(std::string filename) {
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
void Theme::ParseVariablesInScript(const T &variables, E &scriptEngine) {
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
void Theme::SetNamedColorsFromScript(NamedColorConfig &dstColorConfig, const T &globals, E &scriptEngine) {
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


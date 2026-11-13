//
// Created by gnilk on 22.07.23.
//

// TO-DO: Consider supporting Helix TOML files..

#include <nlohmann/json.hpp>

#include "Core/Sublime/SublimeConfigColorScript.h"
#include "Theme.h"

#include "logger.h"
#include "Core/RuntimeConfig.h"

using json = nlohmann::json;
using namespace gedit;

Theme::Ref Theme::Create() {
    auto instance = std::make_shared<Theme>();
    if (!instance->Initialize()) {
        return nullptr;
    }
    return instance;
}

Theme::Ref Theme::LoadOrDefault(const std::string &themeFile, std::string &outProblem) {
    outProblem.clear();
    auto theme = Theme::Create();
    if (theme->Load(themeFile)) {
        return theme;
    }
    outProblem = "theme " + themeFile + ": " + theme->GetLoadError();
    if (themeFile == DEFAULT_THEME) {
        return nullptr;
    }
    // A fresh one - the failed load may have left some colours behind
    theme = Theme::Create();
    if (!theme->Load(DEFAULT_THEME)) {
        outProblem += "; the default theme " + std::string(DEFAULT_THEME) + ": " + theme->GetLoadError();
        return nullptr;
    }
    outProblem += " - using " + std::string(DEFAULT_THEME);
    return theme;
}

bool Theme::Initialize() {
    logger = gnilk::Logger::GetLogger("Theme");
    return true;
}

// Into a fresh theme first, so a file broken since the last load leaves this one as it was
bool Theme::Reload() {
    if  (filename.empty()) {
        logger->Error("Theme must first be loaded");
        return false;
    }
    auto fresh = Theme::Create();
    if (!fresh->Load(filename)) {
        loadError = fresh->loadError;
        logger->Error("Reload of theme '%s' failed: %s - keeping the colours it had", filename.c_str(), loadError.c_str());
        return false;
    }
    dataNode.reset(fresh->dataNode);
    name = fresh->name;
    colorConfig = fresh->colorConfig;
    loadError.clear();
    return true;
}

const NamedColors &Theme::ColorsOrEmpty(const std::string &clrClass) const {
    static const NamedColors noColors;
    auto it = colorConfig.find(clrClass);
    if ((it == colorConfig.end()) || (it->second == nullptr)) {
        return noColors;
    }
    return *it->second;
}


bool Theme::Load(const std::string &pathname) {
    loadError.clear();
    // The parsers throw on a broken file; that's a reason to report, not a crash
    try {
        if (!LoadThemeFile(pathname)) {
            logger->Error("Theme '%s': %s", pathname.c_str(), loadError.c_str());
            return false;
        }
    } catch (const std::exception &e) {
        loadError = std::string("can't be parsed: ") + e.what();
        logger->Error("Theme '%s': %s", pathname.c_str(), loadError.c_str());
        return false;
    }
    InheritGlobalColors();

    logger->Debug("Ok, theme '%s' loaded from file: %s", name.c_str(), pathname.c_str());

    filename = pathname;
    return true;
}

bool Theme::LoadThemeFile(const std::string &pathname) {
    if (!ConfigNode::LoadConfig(pathname)) {
        loadError = "no such theme file";
        return false;
    }

    name = GetStr("name","noname");

    if (!HasKey("colorfile")) {
        loadError = "the theme file has no 'colorfile:'";
        return false;
    }

    auto colorFile = GetStr("colorfile", "colors.json");
    if (!LoadSublimeColorFile(colorFile)) {
        loadError = "its colour file '" + colorFile + "' is missing";
        return false;
    }
    if (!HasColorsForClass(clrClassGlobals) || !HasColorsForClass(clrClassContent)) {
        loadError = "its colour file '" + colorFile + "' has no 'globals' or no 'content' colours";
        return false;
    }
    return true;
}

// One background for the editor: a section that sets no background/foreground of its own takes the global one, so a
// theme sets it once and the text area and terminal follow. A section can still set its own (a darker terminal).
void Theme::InheritGlobalColors() {
    if (!HasColorsForClass(clrClassGlobals)) {
        return;
    }
    auto globals = colorConfig[clrClassGlobals];
    for (auto &clrClass : {clrClassContent, clrClassTerminal}) {
        if (!HasColorsForClass(clrClass)) {
            colorConfig[clrClass] = NamedColors::Create();
        }
        auto colors = colorConfig[clrClass];
        for (auto name : {"background", "foreground"}) {
            if (!colors->HasColor(name) && globals->HasColor(name)) {
                colors->SetColor(name, globals->GetColor(name));
            }
        }
    }
}

bool Theme::IsValidColorClass(const std::string &name) {
    if (name == clrClassContent) return true;
    if (name == clrClassGlobals) return true;
    if (name == clrClassUI) return true;
    if (name == clrClassTerminal) return true;
    return false;
}

bool Theme::LoadSublimeColorFile(std::string filename) {
    // Only enough here - or do we need this one out side???
    SublimeConfigColorScript scriptEngine;
    scriptEngine.RegisterBuiltIn();

    logger->Debug("Loading Sublime Color file: %s", filename);


    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto colorFileAsset = assetLoader.LoadTextAsset(filename);
    if (colorFileAsset == nullptr) {
        logger->Error("Failed to load color file!");
        return false;
    }
    json data = json::parse(colorFileAsset->GetPtrAs<char *>());

    ParseVariablesInScript<json, SublimeConfigColorScript>(data["variables"], scriptEngine);
    logger->Debug("Mapping color variables to sections");
    for (auto &colorSection : data["colors"].items()) {
        if (!colorSection.value().is_object()) {
            logger->Debug("Colors should only contain objects!");
            continue;
        }
        if (!IsValidColorClass(colorSection.key())) {
            logger->Error("Section '%s' is not a valid section!");
            continue;
        }
        logger->Debug("%s", colorSection.key());
        NamedColors::Ref colors = nullptr;
        if (!HasColorsForClass(colorSection.key())) {
            colors = NamedColors::Create();
            colorConfig[colorSection.key()] = colors;
        } else {
            colors = colorConfig[colorSection.key()];
        }

        SetNamedColorsFromScript<json, SublimeConfigColorScript>(colors, colorSection.value(), scriptEngine);
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
void Theme::SetNamedColorsFromScript(NamedColors::Ref dstColorConfig, const T &globals, E &scriptEngine) {
    for(auto &col : globals.items()) {
        if (col.value().is_string()) {
            auto value = col.value().template get<std::string>();
            auto [ok, scriptValue] = scriptEngine.ExecuteScript(value);
            if (ok && scriptValue.IsColor()) {
                dstColorConfig->SetColor(col.key(), scriptValue.Color());
            } else {
                logger->Error("  Value for '%s' is not color, constants not supported - skipping\n", col.key().c_str());
            }
        }
    }
}


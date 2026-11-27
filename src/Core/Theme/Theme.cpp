//
// Created by gnilk on 22.07.23.
//

// TO-DO: Consider supporting Helix TOML files..

#include <filesystem>
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

bool Theme::Reload() {
    if  (filename.empty()) {
        logger->Error("Theme must first be loaded");
        return false;
    }
    return Switch(filename);
}

// Into a fresh theme first, so a file that can't be loaded leaves this one as it was
bool Theme::Switch(const std::string &pathname) {
    auto fresh = Theme::Create();
    if (!fresh->Load(pathname)) {
        loadError = fresh->loadError;
        logger->Error("Theme '%s' can't be loaded: %s - keeping the colours it had", pathname.c_str(), loadError.c_str());
        return false;
    }
    dataNode.reset(fresh->dataNode);
    name = fresh->name;
    colorConfig = fresh->colorConfig;
    contentFontStyles = fresh->contentFontStyles;
    filename = pathname;
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


kTextAttributes Theme::GetContentFontStyle(const std::string &key) const {
    auto it = contentFontStyles.find(key);
    return (it == contentFontStyles.end()) ? kTextAttributes::kNormal : it->second;
}

std::optional<kTextAttributes> Theme::ParseFontStyle(const std::string &words) {
    auto attributes = kTextAttributes::kNormal;
    size_t pos = 0;
    while (pos < words.size()) {
        auto start = words.find_first_not_of(" \t", pos);
        if (start == std::string::npos) {
            break;
        }
        auto end = words.find_first_of(" \t", start);
        end = (end == std::string::npos) ? words.size() : end;
        auto word = words.substr(start, end - start);
        if (word == "bold") {
            attributes = attributes | kTextAttributes::kBold;
        } else if (word == "italic") {
            attributes = attributes | kTextAttributes::kItalic;
        } else if (word == "underline") {
            attributes = attributes | kTextAttributes::kUnderline;
        } else if (word == "strikethrough") {
            attributes = attributes | kTextAttributes::kStrikethrough;
        } else {
            return std::nullopt;
        }
        pos = end;
    }
    return attributes;
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

std::optional<Theme::Location> Theme::Locate(const std::string &fileName, const std::optional<Location> &nextTo) {
    using kLocationType = AssetLoaderBase::kLocationType;
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    if (std::filesystem::path(fileName).is_absolute()) {
        std::error_code ec;
        if (!std::filesystem::exists(fileName, ec)) {
            return {};
        }
        return Location{fileName, kLocationType::kAny};
    }
    std::vector<Location> candidates;
    if (nextTo.has_value()) {
        auto dir = std::filesystem::path(nextTo->name).parent_path();
        candidates.push_back({(dir / fileName).generic_string(), nextTo->locationType});
    }
    candidates.push_back({std::string(USER_THEME_DIR) + "/" + fileName, kLocationType::kUser});
    candidates.push_back({fileName, kLocationType::kUser});
    candidates.push_back({std::string(SYSTEM_THEME_DIR) + "/" + fileName, kLocationType::kSystem});
    candidates.push_back({fileName, kLocationType::kSystem});
    for (auto &candidate : candidates) {
        if (candidate.locationType == kLocationType::kAny) {
            std::error_code ec;
            if (std::filesystem::exists(candidate.name, ec)) {
                return candidate;
            }
            continue;
        }
        if (!assetLoader.ResolveExistingPath(candidate.name, candidate.locationType).empty()) {
            return candidate;
        }
    }
    return {};
}

bool Theme::LoadThemeFile(const std::string &pathname) {
    auto location = Locate(pathname);
    if (!location.has_value() || !ConfigNode::LoadConfig(location->name, location->locationType)) {
        loadError = "no such theme file";
        return false;
    }

    name = GetStr("name","noname");

    if (!HasKey("colorfile")) {
        loadError = "the theme file has no 'colorfile:'";
        return false;
    }

    auto colorFile = GetStr("colorfile", "colors.json");
    auto colorLocation = Locate(colorFile, location);
    if (!colorLocation.has_value() || !LoadSublimeColorFile(*colorLocation)) {
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
        for (auto colorName : {"background", "foreground"}) {
            if (!colors->HasColor(colorName) && globals->HasColor(colorName)) {
                colors->SetColor(colorName, globals->GetColor(colorName));
            }
        }
    }
}

bool Theme::IsValidColorClass(const std::string &className) {
    if (className == clrClassContent) return true;
    if (className == clrClassGlobals) return true;
    if (className == clrClassUI) return true;
    if (className == clrClassTerminal) return true;
    return false;
}

bool Theme::LoadSublimeColorFile(const Location &colorFile) {
    // Only enough here - or do we need this one out side???
    SublimeConfigColorScript scriptEngine;
    scriptEngine.RegisterBuiltIn();

    logger->Debug("Loading Sublime Color file: %s", colorFile.name.c_str());

    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto colorFileAsset = assetLoader.LoadTextAsset(colorFile.name, colorFile.locationType);
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
    if (data.contains(FONT_STYLES)) {
        LoadFontStyles(data[FONT_STYLES]);
    }
    return true;

}

// A style the editor can't draw is left out (the text keeps its colour) - a theme from a newer editor still loads
template<typename T>
void Theme::LoadFontStyles(const T &fontStyles) {
    if (!fontStyles.is_object()) {
        logger->Warning("'%s' is not a table - no font styles", FONT_STYLES);
        return;
    }
    for (auto &item : fontStyles.items()) {
        auto style = item.value().is_string() ? ParseFontStyle(item.value().template get<std::string>()) : std::nullopt;
        if (!style.has_value()) {
            logger->Warning("%s.%s: not a font style (bold, italic, underline, strikethrough) - skipped", FONT_STYLES, item.key().c_str());
            continue;
        }
        contentFontStyles[item.key()] = *style;
    }
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


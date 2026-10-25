//
// Theme importer (code index step 1c) - VS Code / Sublime theme -> GoatEdit colors.json
//
#include <testinterface.h>
#include <cmath>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "Core/Theme/ThemeImporter.h"
#include "Core/Sublime/SublimeConfigColorScript.h"
#include "Core/Language/LanguageTokenClass.h"
#include "Core/RuntimeConfig.h"

using namespace gedit;
using json = nlohmann::json;

extern "C" {
DLL_EXPORT int test_themeimport(ITesting *t);
DLL_EXPORT int test_themeimport_trailingcommas(ITesting *t);
DLL_EXPORT int test_themeimport_vscode(ITesting *t);
DLL_EXPORT int test_themeimport_specificity(ITesting *t);
DLL_EXPORT int test_themeimport_context(ITesting *t);
DLL_EXPORT int test_themeimport_semantic(ITesting *t);
DLL_EXPORT int test_themeimport_overlay(ITesting *t);
DLL_EXPORT int test_themeimport_include(ITesting *t);
DLL_EXPORT int test_themeimport_sublime(ITesting *t);
DLL_EXPORT int test_themeimport_complete(ITesting *t);
DLL_EXPORT int test_themeimport_rejects(ITesting *t);
}

using ThemeColors = std::map<std::string, std::map<std::string, ColorRGBA>>;

// Reads colors.json the way Theme::LoadSublimeColorFile does - variables first, then each section's keys. False
// when a value isn't a colour the editor can read.
static bool LoadColors(const std::string &colorsJson, ThemeColors &colors) {
    SublimeConfigColorScript scriptEngine;
    scriptEngine.RegisterBuiltIn();
    auto data = json::parse(colorsJson);
    bool isOk = true;
    for (auto &variable : data["variables"].items()) {
        auto [isColor, color] = scriptEngine.ExecuteColorScript(variable.value().get<std::string>());
        if (!isColor) {
            printf("variable not a colour: %s = %s\n", variable.key().c_str(), variable.value().get<std::string>().c_str());
            isOk = false;
        }
        scriptEngine.AddVarFromValue<ColorRGBA>(variable.key(), SublimeConfigScriptEngine::kColor, color);
    }
    for (auto &section : data["colors"].items()) {
        for (auto &entry : section.value().items()) {
            auto [isValid, value] = scriptEngine.ExecuteScript(entry.value().get<std::string>());
            if (!isValid || !value.IsColor()) {
                printf("not a colour: %s.%s = %s\n", section.key().c_str(), entry.key().c_str(), entry.value().get<std::string>().c_str());
                isOk = false;
                continue;
            }
            colors[section.key()][entry.key()] = value.Color();
        }
    }
    return isOk;
}

static bool Import(const std::string &text, ThemeColors &colors) {
    auto result = ThemeImporter::Import(text);
    for (auto &line : result.report) {
        printf("  report: %s\n", line.c_str());
    }
    return result.isOk && LoadColors(result.colorsJson, colors);
}

static bool IsColor(const ColorRGBA &color, int red, int green, int blue) {
    auto near = [](float value, int expected) { return std::abs(value * 255.0f - (float)expected) < 0.5f; };
    return near(color.R(), red) && near(color.G(), green) && near(color.B(), blue);
}

DLL_EXPORT int test_themeimport(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_themeimport_trailingcommas(ITesting *t) {
    auto text = std::string(
        "{\n"
        "  // a comment, with a comma ]\n"
        "  \"a\": \"x,}\", /* c, } */\n"
        "  \"b\": [1, 2, ],\n"
        "  \"c\": { \"d\": \"\\\",]\", },\n"
        "}\n");
    auto cleaned = ThemeImporter::RemoveTrailingCommas(text);
    auto data = json::parse(cleaned, nullptr, false, true);
    TR_ASSERT(t, !data.is_discarded());
    TR_ASSERT(t, data["a"] == "x,}");
    TR_ASSERT(t, data["b"].size() == 2);
    TR_ASSERT(t, data["c"]["d"] == "\",]");
    return kTR_Pass;
}

DLL_EXPORT int test_themeimport_vscode(ITesting *t) {
    ThemeColors colors;
    TR_ASSERT(t, Import(R"json({
        "name": "Test",
        "colors": {
            "editor.background": "#102030",
            "editor.foreground": "#c0c0c0",
            "editorCursor.foreground": "#fff",
            "editorLineNumber.foreground": "#405060",
            "terminal.ansiRed": "#aa0000",
        },
        "tokenColors": [
            { "scope": "comment", "settings": { "foreground": "#608060", "fontStyle": "italic" } },
            { "scope": ["string", "constant.numeric"], "settings": { "foreground": "#e0a000" } },
            { "scope": "keyword", "settings": { "foreground": "#c080ff" } },
            { "scope": "storage.type", "settings": { "foreground": "#ff6060" } },
            { "scope": "entity.name.function", "settings": { "foreground": "#80a0ff" } },
        ],
    })json", colors));
    auto &content = colors["content"];
    TR_ASSERT(t, IsColor(content["background"], 0x10, 0x20, 0x30));
    TR_ASSERT(t, IsColor(content["regular"], 0xc0, 0xc0, 0xc0));
    // '#fff' is short for '#ffffff' (the colour script itself reads it as #0f0f0f)
    TR_ASSERT(t, IsColor(content["caret"], 0xff, 0xff, 0xff));
    TR_ASSERT(t, IsColor(content["line_comment"], 0x60, 0x80, 0x60));
    TR_ASSERT(t, IsColor(content["block_comment"], 0x60, 0x80, 0x60));
    TR_ASSERT(t, IsColor(content["string"], 0xe0, 0xa0, 0x00));
    TR_ASSERT(t, IsColor(content["number"], 0xe0, 0xa0, 0x00));
    TR_ASSERT(t, IsColor(content["keyword"], 0xc0, 0x80, 0xff));
    TR_ASSERT(t, IsColor(content["known_type"], 0xff, 0x60, 0x60));
    TR_ASSERT(t, IsColor(content["function"], 0x80, 0xa0, 0xff));
    // The include path is a string in a C/C++ grammar
    TR_ASSERT(t, IsColor(content["import"], 0xe0, 0xa0, 0x00));
    // 'globals' carries the same colours
    TR_ASSERT(t, IsColor(colors["globals"]["keyword"], 0xc0, 0x80, 0xff));
    TR_ASSERT(t, IsColor(colors["ui"]["gutter_foreground"], 0x40, 0x50, 0x60));
    TR_ASSERT(t, IsColor(colors["terminal"]["1"], 0xaa, 0x00, 0x00));
    TR_ASSERT(t, IsColor(colors["terminal"]["2"], 0x00, 0xcd, 0x00));
    return kTR_Pass;
}

DLL_EXPORT int test_themeimport_specificity(ITesting *t) {
    ThemeColors colors;
    TR_ASSERT(t, Import(R"json({
        "colors": { "editor.background": "#000000", "editor.foreground": "#ffffff" },
        "tokenColors": [
            { "scope": "keyword.control", "settings": { "foreground": "#000001" } },
            { "scope": "keyword", "settings": { "foreground": "#000002" } },
            { "scope": "keyword.control", "settings": { "foreground": "#000003" } },
            { "scope": "keyword.control.ruby", "settings": { "foreground": "#000004" } },
            { "scope": "keyword.control", "settings": { "fontStyle": "bold" } },
            { "scope": "source.go keyword.operator", "settings": { "foreground": "#000005" } },
            { "scope": "source.cpp keyword.operator", "settings": { "foreground": "#000006" } },
        ]
    })json", colors));
    auto &content = colors["content"];
    // More specific beats general, the later of two equal ones wins, another language's scope doesn't apply,
    // and a font-style-only rule doesn't take the colour away
    TR_ASSERT(t, IsColor(content["keyword"], 0, 0, 3));
    // 'source.go ...' rules nothing for C/C++; 'source.cpp ...' is just the C/C++ rule
    TR_ASSERT(t, IsColor(content["operator"], 0, 0, 6));
    return kTR_Pass;
}

DLL_EXPORT int test_themeimport_context(ITesting *t) {
    ThemeColors colors;
    TR_ASSERT(t, Import(R"json({
        "colors": { "editor.background": "#000000", "editor.foreground": "#ffffff" },
        "tokenColors": [
            { "scope": "meta.class entity.name.type.class", "settings": { "foreground": "#000001" } },
            { "scope": "entity.name.function", "settings": { "foreground": "#000003" } },
            { "scope": "meta.function-call entity.name.function", "settings": { "foreground": "#000002" } },
            { "scope": "string - string.regexp", "settings": { "foreground": "#000004" } },
        ]
    })json", colors));
    auto &content = colors["content"];
    // A context rule is used only where no context-free rule matches
    TR_ASSERT(t, IsColor(content["user_type"], 0, 0, 1));
    TR_ASSERT(t, IsColor(content["function"], 0, 0, 3));
    // An exclusion is read as the selector it narrows
    TR_ASSERT(t, IsColor(content["string"], 0, 0, 4));
    return kTR_Pass;
}

DLL_EXPORT int test_themeimport_semantic(ITesting *t) {
    ThemeColors colors;
    TR_ASSERT(t, Import(R"json({
        "colors": { "editor.background": "#000000", "editor.foreground": "#ffffff" },
        "tokenColors": [
            { "scope": "entity.name.type.class", "settings": { "foreground": "#000001" } },
            { "scope": "variable.other.property", "settings": { "foreground": "#000002" } },
        ],
        "semanticTokenColors": {
            "class": "#000010",
            "enumMember": { "foreground": "#000011" },
            "property.readonly": "#000012",
            "namespace:python": "#000013",
            "macro:cpp": "#000014",
        }
    })json", colors));
    auto &content = colors["content"];
    TR_ASSERT(t, IsColor(content["user_type"], 0, 0, 0x10));
    TR_ASSERT(t, IsColor(content["enum_value"], 0, 0, 0x11));
    // A modifier has no GoatEdit class, and another language's entry doesn't apply - the scope rules decide
    TR_ASSERT(t, IsColor(content["member"], 0, 0, 2));
    TR_ASSERT(t, IsColor(content["namespace"], 0xff, 0xff, 0xff));
    TR_ASSERT(t, IsColor(content["macro"], 0, 0, 0x14));
    return kTR_Pass;
}

// GoatEdit paints the selection over the text: an opaque source colour must become translucent, and over the
// background still give the colour the theme chose
DLL_EXPORT int test_themeimport_overlay(ITesting *t) {
    ThemeColors colors;
    TR_ASSERT(t, Import(R"json({
        "colors": {
            "editor.background": "#011627",
            "editor.foreground": "#d6deeb",
            "editor.selectionBackground": "#1d3b53",
            "editor.findMatchHighlightBackground": "#1085bb5d",
        }
    })json", colors));
    auto &content = colors["content"];
    auto &selection = content["selection"];
    auto &background = content["background"];
    TR_ASSERT(t, selection.A() < 0.99f);
    TR_ASSERT(t, selection.A() > 0.0f);
    auto blend = [&](float over, float under) { return (over * selection.A() + under * (1.0f - selection.A())) * 255.0f; };
    TR_ASSERT(t, std::abs(blend(selection.R(), background.R()) - 0x1d) < 1.5f);
    TR_ASSERT(t, std::abs(blend(selection.G(), background.G()) - 0x3b) < 1.5f);
    TR_ASSERT(t, std::abs(blend(selection.B(), background.B()) - 0x53) < 1.5f);
    // A colour that already has alpha is kept as the theme wrote it
    TR_ASSERT(t, IsColor(content["search"], 0x10, 0x85, 0xbb));
    TR_ASSERT(t, std::abs(content["search"].A() - (float)0x5d / 255.0f) < 0.01f);
    return kTR_Pass;
}

DLL_EXPORT int test_themeimport_include(ITesting *t) {
    auto parent = std::string(R"json({
        "colors": { "editor.background": "#101010", "editor.foreground": "#202020" },
        "tokenColors": [
            { "scope": "keyword", "settings": { "foreground": "#000001" } },
            { "scope": "string", "settings": { "foreground": "#000002" } },
        ]
    })json");
    std::string requested;
    auto loader = [&](const std::string &path) -> std::optional<std::string> {
        requested = path;
        return parent;
    };
    auto result = ThemeImporter::Import(R"json({
        "include": "./parent.json",
        "colors": { "editor.foreground": "#303030" },
        "tokenColors": [ { "scope": "keyword", "settings": { "foreground": "#000003" } } ]
    })json", loader);
    TR_ASSERT(t, result.isOk);
    TR_ASSERT(t, requested == "./parent.json");
    ThemeColors colors;
    TR_ASSERT(t, LoadColors(result.colorsJson, colors));
    auto &content = colors["content"];
    TR_ASSERT(t, IsColor(content["background"], 0x10, 0x10, 0x10));
    TR_ASSERT(t, IsColor(content["foreground"], 0x30, 0x30, 0x30));
    TR_ASSERT(t, IsColor(content["keyword"], 0, 0, 3));
    TR_ASSERT(t, IsColor(content["string"], 0, 0, 2));

    // A missing parent is reported, the rest still imports
    auto missing = ThemeImporter::Import(R"json({ "include": "./gone.json", "colors": { "editor.background": "#000000" } })json",
                                         [](const std::string &) -> std::optional<std::string> { return std::nullopt; });
    TR_ASSERT(t, missing.isOk);
    bool isReported = false;
    for (auto &line : missing.report) {
        isReported |= (line.find("gone.json") != std::string::npos);
    }
    TR_ASSERT(t, isReported);
    return kTR_Pass;
}

DLL_EXPORT int test_themeimport_sublime(ITesting *t) {
    auto result = ThemeImporter::Import(R"json({
        "name": "Sub",
        "variables": {
            "bg": "hsl(210, 15%, 22%)",
            "orange": "#f9ae58",
            "green": "rgb(153, 199, 148)",
        },
        "globals": {
            "background": "var(bg)",
            "foreground": "#d8dee9",
            "selection": "#4c5866",
        },
        "rules": [
            { "scope": "keyword", "foreground": "var(orange)", "font_style": "italic" },
            { "scope": "string", "foreground": "var(green)" },
            { "scope": "comment", "foreground": "color(var(orange), alpha(0.5))" },
        ]
    })json");
    for (auto &line : result.report) {
        printf("  report: %s\n", line.c_str());
    }
    TR_ASSERT(t, result.isOk);
    TR_ASSERT(t, result.name == "Sub");
    // The scheme's variables stay as written, so the result is tuned in the same terms
    auto data = json::parse(result.colorsJson);
    TR_ASSERT(t, data["variables"]["bg"] == "hsl(210, 15%, 22%)");
    TR_ASSERT(t, data["colors"]["content"]["keyword"] == "var(orange)");
    ThemeColors colors;
    TR_ASSERT(t, LoadColors(result.colorsJson, colors));
    auto &content = colors["content"];
    TR_ASSERT(t, IsColor(content["keyword"], 0xf9, 0xae, 0x58));
    TR_ASSERT(t, IsColor(content["string"], 153, 199, 148));
    TR_ASSERT(t, content["line_comment"].A() < 0.6f);
    TR_ASSERT(t, content["selection"].A() < 0.99f);
    return kTR_Pass;
}

// Every colour GoatEdit reads is in the result - token classes and the keys the stock colors.json defines
DLL_EXPORT int test_themeimport_complete(ITesting *t) {
    auto result = ThemeImporter::Import(R"json({ "colors": { "editor.background": "#000000" } })json");
    TR_ASSERT(t, result.isOk);
    ThemeColors colors;
    TR_ASSERT(t, LoadColors(result.colorsJson, colors));
    for (int i = 0; IsLanguageTokenClass(i); i++) {
        auto key = LanguageTokenClassToString(static_cast<kLanguageTokenClass>(i));
        if (!colors["content"].contains(key)) {
            printf("missing content key: %s\n", key.c_str());
        }
        TR_ASSERT(t, colors["content"].contains(key));
    }
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto stockAsset = assetLoader.LoadTextAsset("colors.json");
    TR_ASSERT(t, stockAsset != nullptr);
    auto stock = json::parse(stockAsset->GetPtrAs<char *>());
    for (auto &section : stock["colors"].items()) {
        for (auto &entry : section.value().items()) {
            if (!colors[section.key()].contains(entry.key())) {
                printf("missing %s.%s\n", section.key().c_str(), entry.key().c_str());
            }
            TR_ASSERT(t, colors[section.key()].contains(entry.key()));
        }
    }
    return kTR_Pass;
}

DLL_EXPORT int test_themeimport_rejects(ITesting *t) {
    TR_ASSERT(t, !ThemeImporter::Import("<?xml version=\"1.0\"?><plist></plist>").isOk);
    TR_ASSERT(t, !ThemeImporter::Import("{ not json").isOk);
    TR_ASSERT(t, !ThemeImporter::Import(R"json({ "something": "else" })json").isOk);
    return kTR_Pass;
}

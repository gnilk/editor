//
// VS Code / Sublime theme -> GoatEdit colors.json - see ThemeImporter.h
//

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "Core/ColorRGBA.h"
#include "Core/Sublime/SublimeConfigColorScript.h"
#include "ThemeImporter.h"

using namespace gedit;
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

namespace {
    // A colour as written to colors.json ('expr') and what it evaluates to
    struct ColorValue {
        std::string expr;
        ColorRGBA color;
    };

    // One selector of a scope rule, reduced to the scope it colours
    struct ScopeRule {
        std::string scope;          // the last element of the selector
        std::string selector;       // as written, for the report
        std::string color;          // foreground expression
        bool isContext = false;     // the selector needs ancestors GoatEdit can't see ('meta.class entity.name')
        size_t order = 0;           // later rules win a tie
    };

    // A GoatEdit content key and the scopes its tokens would carry in a C/C++ grammar, best first; 'fallback' is
    // the key it copies when no rule matches
    struct ContentKey {
        const char *key;
        std::vector<const char *> scopes;
        const char *fallback;
    };

    // A GoatEdit key and the source colour ids it takes, best first
    struct MappedKey {
        const char *key;
        std::vector<const char *> sourceIds;
        const char *fallback;       // a key already resolved in the same section (or nullptr)
        bool isOverlay = false;     // drawn over the text - must be translucent
    };

    // The theme file, reduced to what the importer reads
    struct SourceTheme {
        bool isSublime = false;
        std::string name;
        std::vector<std::pair<std::string, std::string>> variables;     // Sublime only, in order
        std::map<std::string, std::string> colors;                      // VS Code 'colors' / Sublime 'globals'
        std::vector<ScopeRule> rules;
        std::map<std::string, std::string> semanticColors;              // content key -> colour
    };

    class Importer {
    public:
        explicit Importer(const ThemeImporter::IncludeLoader &includeLoader) : includeLoader(includeLoader) {
            scriptEngine.RegisterBuiltIn();
        }
        ThemeImporter::Result Run(const std::string &text);
    protected:
        bool ReadTheme(const std::string &text, int depth);
        void ReadVSCodeTheme(const json &data, int depth);
        void ReadSublimeScheme(const json &data);
        void AddScopeRules(const std::string &selectors, const std::string &color);

        std::optional<ColorValue> Evaluate(const std::string &expr);
        void ResolveMappedKeys(const std::string &section, const std::vector<MappedKey> &keys);
        void ResolveContentKeys();
        std::optional<ColorValue> MatchScopes(const ContentKey &contentKey);
        const ScopeRule *BestRule(const std::string &scope, bool allowContext) const;
        void ResolveTerminalPalette();
        ColorValue OverlayFor(const ColorValue &value, const ColorRGBA &background) const;
        ordered_json BuildColorsJson();

        void Report(const std::string &line) { result.report.push_back(line); }
    protected:
        const ThemeImporter::IncludeLoader &includeLoader;
        SublimeConfigColorScript scriptEngine;
        SourceTheme source;
        // Resolved GoatEdit colours per section, in output order
        std::map<std::string, std::vector<std::pair<std::string, ColorValue>>> sections;
        size_t numContextSelectors = 0;
        size_t numExclusions = 0;
        size_t numUnsupportedSelectors = 0;
        size_t numFontStyles = 0;
        ThemeImporter::Result result;
    };

    std::string Trim(const std::string &str) {
        auto first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return {};
        }
        auto last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, last - first + 1);
    }

    std::vector<std::string> Split(const std::string &str, char separator) {
        std::vector<std::string> parts;
        size_t start = 0;
        while (start <= str.size()) {
            auto end = str.find(separator, start);
            if (end == std::string::npos) {
                end = str.size();
            }
            parts.push_back(str.substr(start, end - start));
            start = end + 1;
        }
        return parts;
    }

    std::vector<std::string> SplitWhitespace(const std::string &str) {
        std::vector<std::string> parts;
        std::string part;
        for (auto ch : str) {
            if ((ch == ' ') || (ch == '\t')) {
                if (!part.empty()) {
                    parts.push_back(part);
                    part.clear();
                }
                continue;
            }
            part += ch;
        }
        if (!part.empty()) {
            parts.push_back(part);
        }
        return parts;
    }

    // '#rgb', '#rgba', '#rrggbb' or '#rrggbbaa'
    std::optional<ColorRGBA> ParseHexColor(const std::string &str) {
        if ((str.size() < 4) || (str[0] != '#')) {
            return {};
        }
        auto digits = str.substr(1);
        if (!std::all_of(digits.begin(), digits.end(), [](char ch) { return std::isxdigit((unsigned char)ch); })) {
            return {};
        }
        if ((digits.size() == 3) || (digits.size() == 4)) {
            std::string expanded;
            for (auto ch : digits) {
                expanded += ch;
                expanded += ch;
            }
            digits = expanded;
        }
        if ((digits.size() != 6) && (digits.size() != 8)) {
            return {};
        }
        auto channel = [&digits](size_t idx) { return (int)std::stoul(digits.substr(idx * 2, 2), nullptr, 16); };
        auto alpha = (digits.size() == 8) ? channel(3) : 255;
        return ColorRGBA::FromRGBA(channel(0), channel(1), channel(2), alpha);
    }

    // How colors.json writes a literal colour - the colour script reads '#rrggbb' and 'rgba()', not '#rrggbbaa'
    // (rounded - ColorRGBA's 'AsInt' truncates, which turns 29/255 back into 28)
    std::string ColorExpr(const ColorRGBA &color) {
        auto channel = [](float value) { return (int)std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f); };
        if (channel(color.A()) >= 255) {
            return fmt::format("#{:02x}{:02x}{:02x}", channel(color.R()), channel(color.G()), channel(color.B()));
        }
        return fmt::format("rgba({}, {}, {}, {:.2f})", channel(color.R()), channel(color.G()), channel(color.B()), color.A());
    }

    bool IsLiteral(const std::string &expr) {
        return expr.find("var(") == std::string::npos;
    }

    // An element of a selector matches a scope when it is the scope or a dot-separated prefix of it
    bool ElementMatches(const std::string &element, const std::string &scope) {
        if (scope.compare(0, element.size(), element) != 0) {
            return false;
        }
        return (scope.size() == element.size()) || (scope[element.size()] == '.');
    }

    size_t Specificity(const std::string &element) {
        return std::count(element.begin(), element.end(), '.') + 1;
    }

    // A leading 'source.x' that names C/C++ narrows nothing for GoatEdit; another language rules the selector out
    bool IsCPPSource(const std::string &element) {
        static const std::set<std::string> cppSources = {"source.c", "source.c++", "source.cpp", "source.objc", "source.objc++"};
        return cppSources.contains(element);
    }

    std::string ColorFromSetting(const json &value) {
        if (value.is_string()) {
            return value.get<std::string>();
        }
        if (value.is_object() && value.contains("foreground") && value["foreground"].is_string()) {
            return value["foreground"].get<std::string>();
        }
        return {};
    }

    const std::vector<ContentKey> contentKeys = {
        {"operator",         {"keyword.operator.cpp", "punctuation"}, "regular"},
        {"keyword",          {"keyword.control.cpp", "storage.modifier.cpp"}, "regular"},
        {"separator",        {"punctuation.separator.delimiter.cpp"}, "operator"},
        {"known_type",       {"storage.type.built-in.primitive.cpp", "storage.type.cpp", "support.type"}, "keyword"},
        {"number",           {"constant.numeric.decimal.cpp"}, "regular"},
        {"string",           {"string.quoted.double.cpp"}, "regular"},
        {"line_comment",     {"comment.line.double-slash.cpp"}, "regular"},
        {"block_comment",    {"comment.block.cpp"}, "line_comment"},
        {"commented_text",   {"comment.block.preprocessor.if-branch.cpp"}, "block_comment"},
        {"import",           {"string.quoted.other.lt-gt.include.cpp", "string.quoted.double.include.cpp"}, "string"},
        {"preprocessor",     {"keyword.control.directive.cpp", "meta.preprocessor.cpp"}, "keyword"},
        {"macro_identifier", {"entity.name.function.preprocessor.cpp"}, "preprocessor"},
        {"heading",          {"markup.heading.markdown", "entity.name.section.markdown"}, "keyword"},
        {"strong",           {"markup.bold.markdown"}, "regular"},
        {"emphasis",         {"markup.italic.markdown"}, "regular"},
        {"code",             {"markup.inline.raw.string.markdown", "markup.raw"}, "string"},
        {"list_marker",      {"punctuation.definition.list.begin.markdown", "beginning.punctuation.definition.list.markdown"}, "operator"},
        {"block_quote",      {"markup.quote.markdown"}, "line_comment"},
        {"link",             {"markup.underline.link.markdown", "string.other.link"}, "string"},
        {"rule",             {"meta.separator.markdown"}, "line_comment"},
        {"user_type",        {"entity.name.type.class.cpp", "entity.name.class", "entity.name.type", "support.class"}, "known_type"},
        {"function",         {"entity.name.function.cpp", "support.function"}, "regular"},
        {"member",           {"variable.other.property.cpp", "variable.other.member.cpp", "variable.other.object.property"}, "regular"},
        {"macro",            {"entity.name.function.preprocessor.cpp", "entity.name.macro", "support.macro"}, "macro_identifier"},
        {"enum_value",       {"variable.other.enummember.cpp", "constant.other.enum", "constant.other"}, "number"},
        {"namespace",        {"entity.name.namespace.cpp", "entity.name.type.namespace"}, "regular"},
        {"hexview",          {}, "line_comment"},
        {"unknown",          {}, "regular"},
        {"funky",            {}, "regular"},
    };

    // VS Code semantic token types (semanticTokenColors) -> content key
    const std::vector<std::pair<const char *, const char *>> semanticTypes = {
        {"class", "user_type"}, {"struct", "user_type"}, {"type", "user_type"}, {"enum", "user_type"},
        {"interface", "user_type"}, {"function", "function"}, {"method", "function"}, {"property", "member"},
        {"enumMember", "enum_value"}, {"namespace", "namespace"}, {"macro", "macro"},
    };

    // Content colours that aren't tokens (the token keys come from the scope rules)
    const std::vector<MappedKey> vscodeEditorKeys = {
        {"background", {"editor.background"}, nullptr},
        {"foreground", {"editor.foreground", "foreground"}, nullptr},
        {"caret",      {"editorCursor.foreground"}, "foreground"},
        {"selection",  {"editor.selectionBackground", "selection.background"}, nullptr, true},
        {"search",     {"editor.findMatchHighlightBackground", "editor.findMatchBackground"}, "selection", true},
    };
    const std::vector<MappedKey> sublimeEditorKeys = {
        {"background", {"background"}, nullptr},
        {"foreground", {"foreground"}, nullptr},
        {"caret",      {"caret"}, "foreground"},
        {"selection",  {"selection"}, nullptr, true},
        {"search",     {"find_highlight"}, "selection", true},
    };
    const std::vector<MappedKey> vscodeUIKeys = {
        {"background",               {"sideBar.background", "editor.background"}, nullptr},
        {"foreground",               {"sideBar.foreground", "foreground", "editor.foreground"}, nullptr},
        {"gutter_background",        {"editorGutter.background", "editor.background"}, "background"},
        {"gutter_foreground",        {"editorLineNumber.foreground"}, "foreground"},
        {"header_background",        {"editorGroupHeader.tabsBackground", "tab.inactiveBackground"}, "gutter_background"},
        {"header_foreground",        {"tab.inactiveForeground"}, "gutter_foreground"},
        {"header_active_background", {"tab.activeBackground"}, "header_background"},
        {"header_active_foreground", {"tab.activeForeground"}, "foreground"},
        {"modal_background",         {"quickInput.background", "editorWidget.background"}, "header_background"},
        {"modal_foreground",         {"quickInput.foreground", "editorWidget.foreground", "foreground"}, "foreground"},
    };
    const std::vector<MappedKey> sublimeUIKeys = {
        {"background",               {"background"}, nullptr},
        {"foreground",               {"foreground"}, nullptr},
        {"gutter_background",        {"gutter"}, "background"},
        {"gutter_foreground",        {"gutter_foreground"}, "foreground"},
        {"header_background",        {}, "gutter_background"},
        {"header_foreground",        {}, "gutter_foreground"},
        {"header_active_background", {"line_highlight"}, "background"},
        {"header_active_foreground", {}, "foreground"},
        {"modal_background",         {}, "gutter_background"},
        {"modal_foreground",         {}, "foreground"},
    };
    const std::vector<MappedKey> vscodeTerminalKeys = {
        {"background", {"terminal.background", "panel.background", "editor.background"}, nullptr},
        {"foreground", {"terminal.foreground", "editor.foreground", "foreground"}, nullptr},
        {"selection",  {"terminal.selectionBackground", "editor.selectionBackground"}, nullptr, true},
    };
    const std::vector<MappedKey> sublimeTerminalKeys = {
        {"background", {"background"}, nullptr},
        {"foreground", {"foreground"}, nullptr},
        {"selection",  {"selection"}, nullptr, true},
    };
    const std::vector<const char *> vscodeAnsiIds = {
        "terminal.ansiBlack", "terminal.ansiRed", "terminal.ansiGreen", "terminal.ansiYellow",
        "terminal.ansiBlue", "terminal.ansiMagenta", "terminal.ansiCyan", "terminal.ansiWhite",
        "terminal.ansiBrightBlack", "terminal.ansiBrightRed", "terminal.ansiBrightGreen", "terminal.ansiBrightYellow",
        "terminal.ansiBrightBlue", "terminal.ansiBrightMagenta", "terminal.ansiBrightCyan", "terminal.ansiBrightWhite",
    };
    // xterm's palette - GoatEdit's default terminal colours
    const std::vector<const char *> defaultAnsiColors = {
        "#000000", "#cd0000", "#00cd00", "#cdcd00", "#0000ee", "#cd00cd", "#00cdcd", "#e5e5e5",
        "#7f7f7f", "#ff0000", "#00ff00", "#ffff00", "#5c5cff", "#ff00ff", "#00ffff", "#ffffff",
    };

    // Overlay colours are painted with alpha over the text; this is the least opacity an opaque import gets
    constexpr float MIN_OVERLAY_ALPHA = 0.35f;
    constexpr float DEFAULT_OVERLAY_ALPHA = 0.25f;
    constexpr int MAX_INCLUDE_DEPTH = 8;
}

ThemeImporter::Result ThemeImporter::Import(const std::string &text, const IncludeLoader &includeLoader) {
    Importer importer(includeLoader);
    return importer.Run(text);
}

ThemeImporter::Result Importer::Run(const std::string &text) {
    if (!ReadTheme(text, 0)) {
        return result;
    }
    result.name = source.name.empty() ? "imported" : source.name;

    // Sublime variables first - rules and globals refer to them
    for (auto &[name, expr] : source.variables) {
        auto value = Evaluate(expr);
        if (!value.has_value()) {
            Report(fmt::format("variable '{}': '{}' isn't a colour GoatEdit can read - dropped", name, expr));
            continue;
        }
        scriptEngine.AddVarFromValue<ColorRGBA>(name, SublimeConfigScriptEngine::kColor, value->color);
    }

    ResolveMappedKeys("content", source.isSublime ? sublimeEditorKeys : vscodeEditorKeys);
    ResolveContentKeys();
    ResolveMappedKeys("ui", source.isSublime ? sublimeUIKeys : vscodeUIKeys);
    ResolveMappedKeys("terminal", source.isSublime ? sublimeTerminalKeys : vscodeTerminalKeys);
    ResolveTerminalPalette();

    if (numContextSelectors > 0) {
        Report(fmt::format("{} selectors need a context GoatEdit doesn't track ('meta.class entity.name...') - used only where nothing else matched", numContextSelectors));
    }
    if (numExclusions > 0) {
        Report(fmt::format("{} selectors have exclusions ('a - b') - read as 'a'", numExclusions));
    }
    if (numUnsupportedSelectors > 0) {
        Report(fmt::format("{} selectors use groups or alternatives ('(a | b)') - dropped", numUnsupportedSelectors));
    }
    if (numFontStyles > 0) {
        Report(fmt::format("{} rules set a font style (bold/italic) - GoatEdit themes have colours only, dropped", numFontStyles));
    }

    result.colorsJson = BuildColorsJson().dump(2);
    result.isOk = true;
    return result;
}

bool Importer::ReadTheme(const std::string &text, int depth) {
    auto firstChar = text.find_first_not_of(" \t\r\n");
    if ((firstChar != std::string::npos) && (text[firstChar] == '<')) {
        Report("this looks like a .tmTheme (XML) - only VS Code themes (.json) and Sublime .sublime-color-scheme are supported");
        return false;
    }
    auto data = json::parse(ThemeImporter::RemoveTrailingCommas(text), nullptr, false, true);
    if (data.is_discarded() || !data.is_object()) {
        Report("not a JSON theme (or it doesn't parse)");
        return false;
    }
    if (source.name.empty() && data.contains("name") && data["name"].is_string()) {
        source.name = data["name"].get<std::string>();
    }
    if (data.contains("rules") || data.contains("globals")) {
        source.isSublime = true;
        ReadSublimeScheme(data);
        return true;
    }
    if (data.contains("tokenColors") || data.contains("colors") || data.contains("include")) {
        ReadVSCodeTheme(data, depth);
        return true;
    }
    Report("neither a VS Code theme ('colors'/'tokenColors') nor a Sublime colour scheme ('globals'/'rules')");
    return false;
}

void Importer::ReadVSCodeTheme(const json &data, int depth) {
    // The parent first - this theme's colours and rules override it
    if (data.contains("include") && data["include"].is_string()) {
        auto includePath = data["include"].get<std::string>();
        auto includeText = includeLoader ? includeLoader(includePath) : std::nullopt;
        if (!includeText.has_value()) {
            Report(fmt::format("included theme '{}' not found - what it defines is missing", includePath));
        } else if (depth >= MAX_INCLUDE_DEPTH) {
            Report(fmt::format("includes nest too deep at '{}' - stopped", includePath));
        } else {
            ReadTheme(*includeText, depth + 1);
        }
    }
    if (data.contains("colors") && data["colors"].is_object()) {
        for (auto &item : data["colors"].items()) {
            if (item.value().is_string()) {
                source.colors[item.key()] = item.value().get<std::string>();
            }
        }
    }
    if (data.contains("tokenColors") && data["tokenColors"].is_string()) {
        Report(fmt::format("token colours live in '{}' (a .tmTheme) - not supported, code colours are missing", data["tokenColors"].get<std::string>()));
    } else if (data.contains("tokenColors") && data["tokenColors"].is_array()) {
        for (auto &rule : data["tokenColors"]) {
            if (!rule.is_object() || !rule.contains("settings") || !rule["settings"].is_object()) {
                continue;
            }
            auto &settings = rule["settings"];
            if (settings.contains("fontStyle") && settings["fontStyle"].is_string() && !Trim(settings["fontStyle"].get<std::string>()).empty()) {
                numFontStyles++;
            }
            auto foreground = settings.contains("foreground") && settings["foreground"].is_string() ? settings["foreground"].get<std::string>() : "";
            // Old-style global settings: a rule without a scope
            if (!rule.contains("scope")) {
                if (!foreground.empty() && !source.colors.contains("editor.foreground")) {
                    source.colors["editor.foreground"] = foreground;
                }
                if (settings.contains("background") && settings["background"].is_string() && !source.colors.contains("editor.background")) {
                    source.colors["editor.background"] = settings["background"].get<std::string>();
                }
                continue;
            }
            // A rule that only sets a font style must not take a colour away
            if (foreground.empty()) {
                continue;
            }
            auto &scope = rule["scope"];
            if (scope.is_string()) {
                AddScopeRules(scope.get<std::string>(), foreground);
            } else if (scope.is_array()) {
                for (auto &selector : scope) {
                    if (selector.is_string()) {
                        AddScopeRules(selector.get<std::string>(), foreground);
                    }
                }
            }
        }
    }
    if (data.contains("semanticTokenColors") && data["semanticTokenColors"].is_object()) {
        for (auto &item : data["semanticTokenColors"].items()) {
            // 'type', 'type:language', 'type.modifier' - modifiers (readonly, static) have no GoatEdit class
            auto selector = item.key();
            auto language = std::string{};
            auto colon = selector.find(':');
            if (colon != std::string::npos) {
                language = selector.substr(colon + 1);
                selector = selector.substr(0, colon);
            }
            if ((selector.find('.') != std::string::npos) || (!language.empty() && (language != "cpp") && (language != "c"))) {
                continue;
            }
            auto color = ColorFromSetting(item.value());
            if (color.empty()) {
                continue;
            }
            for (auto &[semanticType, key] : semanticTypes) {
                // The first type listed for a key wins ('class' before 'struct' for user_type)
                if ((selector == semanticType) && !source.semanticColors.contains(key)) {
                    source.semanticColors[key] = color;
                }
            }
        }
    }
}

void Importer::ReadSublimeScheme(const json &data) {
    if (data.contains("variables") && data["variables"].is_object()) {
        for (auto &item : data["variables"].items()) {
            if (item.value().is_string()) {
                source.variables.emplace_back(item.key(), item.value().get<std::string>());
            }
        }
    }
    if (data.contains("globals") && data["globals"].is_object()) {
        for (auto &item : data["globals"].items()) {
            if (item.value().is_string()) {
                source.colors[item.key()] = item.value().get<std::string>();
            }
        }
    }
    if (data.contains("rules") && data["rules"].is_array()) {
        for (auto &rule : data["rules"]) {
            if (!rule.is_object() || !rule.contains("scope") || !rule["scope"].is_string()) {
                continue;
            }
            if (rule.contains("font_style") && rule["font_style"].is_string() && !Trim(rule["font_style"].get<std::string>()).empty()) {
                numFontStyles++;
            }
            // A gradient foreground (an array) has no single colour
            if (!rule.contains("foreground") || !rule["foreground"].is_string()) {
                continue;
            }
            AddScopeRules(rule["scope"].get<std::string>(), rule["foreground"].get<std::string>());
        }
    }
}

// 'selectors' is a comma separated list; each is a space separated path, optionally with ' - exclusion'
void Importer::AddScopeRules(const std::string &selectors, const std::string &color) {
    for (auto &part : Split(selectors, ',')) {
        auto selector = Trim(part);
        if (selector.empty()) {
            continue;
        }
        if ((selector.find_first_of("()|&") != std::string::npos) || (selector.find(':') != std::string::npos)) {
            numUnsupportedSelectors++;
            continue;
        }
        auto exclusion = selector.find(" -");
        if (exclusion != std::string::npos) {
            numExclusions++;
            selector = Trim(selector.substr(0, exclusion));
        }
        auto path = SplitWhitespace(selector);
        if (path.empty()) {
            continue;
        }
        ScopeRule rule;
        rule.scope = path.back();
        rule.selector = selector;
        rule.color = color;
        rule.order = source.rules.size();
        bool isOtherLanguage = false;
        for (size_t i = 0; i + 1 < path.size(); i++) {
            if (path[i].starts_with("source.") || path[i].starts_with("text.")) {
                isOtherLanguage |= !IsCPPSource(path[i]);
                continue;
            }
            rule.isContext = true;
        }
        if (isOtherLanguage) {
            continue;
        }
        if (rule.isContext) {
            numContextSelectors++;
        }
        source.rules.push_back(rule);
    }
}

std::optional<ColorValue> Importer::Evaluate(const std::string &expr) {
    auto trimmed = Trim(expr);
    // Hex goes through our own reader - the colour script takes neither '#rgb' nor '#rrggbbaa' correctly
    if (!trimmed.empty() && (trimmed[0] == '#')) {
        auto color = ParseHexColor(trimmed);
        if (!color.has_value()) {
            return {};
        }
        return ColorValue{ColorExpr(*color), *color};
    }
    auto [isOk, scriptValue] = scriptEngine.ExecuteScript(trimmed);
    if (!isOk || !scriptValue.IsColor()) {
        return {};
    }
    return ColorValue{trimmed, scriptValue.Color()};
}

void Importer::ResolveMappedKeys(const std::string &section, const std::vector<MappedKey> &keys) {
    auto &resolved = sections[section];
    auto find = [&resolved](const std::string &key) -> const ColorValue * {
        for (auto &[name, value] : resolved) {
            if (name == key) {
                return &value;
            }
        }
        return nullptr;
    };
    auto &contentSection = sections["content"];
    for (auto &mappedKey : keys) {
        std::optional<ColorValue> value;
        for (auto sourceId : mappedKey.sourceIds) {
            auto it = source.colors.find(sourceId);
            if (it == source.colors.end()) {
                continue;
            }
            value = Evaluate(it->second);
            if (!value.has_value()) {
                Report(fmt::format("{}.{}: '{}' isn't a colour GoatEdit can read - skipped", section, mappedKey.key, it->second));
                continue;
            }
            break;
        }
        if (!value.has_value() && (mappedKey.fallback != nullptr) && (find(mappedKey.fallback) != nullptr)) {
            value = *find(mappedKey.fallback);
        }
        // No selection colour at all: a faint tint of the text colour, like Sublime's default
        if (!value.has_value() && mappedKey.isOverlay && (find("foreground") != nullptr)) {
            auto color = find("foreground")->color;
            color.SetAlpha(DEFAULT_OVERLAY_ALPHA);
            value = ColorValue{ColorExpr(color), color};
            Report(fmt::format("{}.{}: nothing in the theme - the foreground at {:.2f} alpha", section, mappedKey.key, DEFAULT_OVERLAY_ALPHA));
        }
        // The background/foreground every theme has - from the editor's if a section lacks its own
        if (!value.has_value() && (&resolved != &contentSection)) {
            for (auto &[name, contentValue] : contentSection) {
                if (name == mappedKey.key) {
                    value = contentValue;
                }
            }
        }
        if (!value.has_value()) {
            value = (std::string(mappedKey.key) == "background") ? ColorValue{"#1e1e1e", ColorRGBA::FromRGB(0x1e, 0x1e, 0x1e)}
                                                                 : ColorValue{"#d4d4d4", ColorRGBA::FromRGB(0xd4, 0xd4, 0xd4)};
            Report(fmt::format("{}.{}: nothing in the theme - GoatEdit default {}", section, mappedKey.key, value->expr));
        }
        if (mappedKey.isOverlay) {
            auto background = find("background");
            value = OverlayFor(*value, (background != nullptr) ? background->color : ColorRGBA::FromRGB(0, 0, 0));
        }
        resolved.emplace_back(mappedKey.key, *value);
    }
}

void Importer::ResolveContentKeys() {
    auto &content = sections["content"];
    auto find = [&content](const std::string &key) -> std::optional<ColorValue> {
        for (auto &[name, value] : content) {
            if (name == key) {
                return value;
            }
        }
        return {};
    };
    content.emplace_back("regular", *find("foreground"));

    std::vector<std::string> fallbacks;
    for (auto &contentKey : contentKeys) {
        std::optional<ColorValue> value;
        auto semantic = source.semanticColors.find(contentKey.key);
        if (semantic != source.semanticColors.end()) {
            value = Evaluate(semantic->second);
        }
        if (!value.has_value()) {
            value = MatchScopes(contentKey);
        }
        if (!value.has_value()) {
            value = find(contentKey.fallback);
            if (!contentKey.scopes.empty()) {
                fallbacks.push_back(fmt::format("{} (as {})", contentKey.key, contentKey.fallback));
            }
        }
        content.emplace_back(contentKey.key, *value);
    }
    if (!fallbacks.empty()) {
        std::string list;
        for (auto &fallback : fallbacks) {
            list += (list.empty() ? "" : ", ") + fallback;
        }
        Report("no rule in the theme for: " + list);
    }
}

// The first scope (best first) any rule matches decides; context-free rules anywhere in the list beat a
// context rule, which only applies where GoatEdit can't check its context
std::optional<ColorValue> Importer::MatchScopes(const ContentKey &contentKey) {
    for (auto allowContext : {false, true}) {
        for (auto scope : contentKey.scopes) {
            auto rule = BestRule(scope, allowContext);
            if (rule == nullptr) {
                continue;
            }
            auto value = Evaluate(rule->color);
            if (!value.has_value()) {
                Report(fmt::format("{}: '{}' (from '{}') isn't a colour GoatEdit can read - skipped", contentKey.key, rule->color, rule->selector));
                continue;
            }
            return value;
        }
    }
    return {};
}

const ScopeRule *Importer::BestRule(const std::string &scope, bool allowContext) const {
    const ScopeRule *best = nullptr;
    for (auto &rule : source.rules) {
        if ((rule.isContext && !allowContext) || !ElementMatches(rule.scope, scope)) {
            continue;
        }
        // '>=' - the later of two equally specific rules wins
        if ((best == nullptr) || (Specificity(rule.scope) >= Specificity(best->scope))) {
            best = &rule;
        }
    }
    return best;
}

void Importer::ResolveTerminalPalette() {
    auto &terminal = sections["terminal"];
    size_t numDefaults = 0;
    for (size_t i = 0; i < defaultAnsiColors.size(); i++) {
        std::optional<ColorValue> value;
        if (!source.isSublime) {
            auto it = source.colors.find(vscodeAnsiIds[i]);
            if (it != source.colors.end()) {
                value = Evaluate(it->second);
            }
        }
        if (!value.has_value()) {
            value = Evaluate(defaultAnsiColors[i]);
            numDefaults++;
        }
        terminal.emplace_back(std::to_string(i), *value);
    }
    if (numDefaults > 0) {
        Report(fmt::format("terminal: {} of 16 ANSI colours not in the theme - xterm defaults", numDefaults));
    }
}

// GoatEdit paints selection and search OVER the text with the colour's alpha; the source paints them under it.
// An opaque colour would hide the text, so it becomes the least translucent tint that still gives the same
// colour over the background: over 'background' the overlay (color * a + background * (1-a)) equals the source.
ColorValue Importer::OverlayFor(const ColorValue &value, const ColorRGBA &background) const {
    if (value.color.A() < 0.99f) {
        return value;
    }
    float target[3] = {value.color.R(), value.color.G(), value.color.B()};
    float under[3] = {background.R(), background.G(), background.B()};
    float alpha = MIN_OVERLAY_ALPHA;
    for (int i = 0; i < 3; i++) {
        if (target[i] > under[i]) {
            alpha = std::max(alpha, (target[i] - under[i]) / (1.0f - under[i]));
        } else if (target[i] < under[i]) {
            alpha = std::max(alpha, (under[i] - target[i]) / under[i]);
        }
    }
    alpha = std::min(1.0f, alpha);
    float tint[3];
    for (int i = 0; i < 3; i++) {
        tint[i] = std::clamp((target[i] - (1.0f - alpha) * under[i]) / alpha, 0.0f, 1.0f);
    }
    auto color = ColorRGBA::FromRGBA((int)std::lround(tint[0] * 255), (int)std::lround(tint[1] * 255), (int)std::lround(tint[2] * 255), (int)std::lround(alpha * 255));
    return {ColorExpr(color), color};
}

// Literal colours become variables named after the first key using them, so a colour is tuned in one place;
// Sublime variables are kept as the scheme wrote them
ordered_json Importer::BuildColorsJson() {
    ordered_json variables = ordered_json::object();
    for (auto &[name, expr] : source.variables) {
        if (scriptEngine.HasVariable(name)) {
            auto value = ParseHexColor(Trim(expr));
            variables[name] = value.has_value() ? ColorExpr(*value) : Trim(expr);
        }
    }
    std::map<std::string, std::string> variableForLiteral;
    auto reference = [&](const std::string &section, const std::string &key, const ColorValue &value) -> std::string {
        if (!IsLiteral(value.expr)) {
            return value.expr;
        }
        auto it = variableForLiteral.find(value.expr);
        if (it != variableForLiteral.end()) {
            return "var(" + it->second + ")";
        }
        auto name = (section == "content") ? key : (section == "terminal" && std::isdigit((unsigned char)key[0])) ? "ansi" + key : section + "_" + key;
        while (variables.contains(name)) {
            name += "_";
        }
        variables[name] = value.expr;
        variableForLiteral[value.expr] = name;
        return "var(" + name + ")";
    };

    ordered_json colors = ordered_json::object();
    ordered_json content = ordered_json::object();
    for (auto &[key, value] : sections["content"]) {
        content[key] = reference("content", key, value);
    }
    // 'globals' is what the toolkit reads for the same keys - the same colours
    colors["globals"] = content;
    colors["content"] = content;
    for (auto section : {"ui", "terminal"}) {
        ordered_json entries = ordered_json::object();
        for (auto &[key, value] : sections[section]) {
            entries[key] = reference(section, key, value);
        }
        colors[section] = entries;
    }

    ordered_json root = ordered_json::object();
    root["name"] = result.name;
    root["author"] = "Imported by goatedit --import-theme";
    root["variables"] = variables;
    root["colors"] = colors;
    return root;
}

std::string ThemeImporter::RemoveTrailingCommas(const std::string &text) {
    std::string out;
    out.reserve(text.size());
    size_t i = 0;
    // Skips whitespace and comments from 'pos' - where the next real character is
    auto nextSignificant = [&text](size_t pos) {
        while (pos < text.size()) {
            if (std::isspace((unsigned char)text[pos])) {
                pos++;
            } else if (text.compare(pos, 2, "//") == 0) {
                pos = text.find('\n', pos);
                pos = (pos == std::string::npos) ? text.size() : pos;
            } else if (text.compare(pos, 2, "/*") == 0) {
                pos = text.find("*/", pos + 2);
                pos = (pos == std::string::npos) ? text.size() : pos + 2;
            } else {
                break;
            }
        }
        return pos;
    };
    while (i < text.size()) {
        auto ch = text[i];
        if (ch == '"') {
            // Copy the string as is, escapes included
            auto start = i++;
            while ((i < text.size()) && (text[i] != '"')) {
                i += (text[i] == '\\') ? 2 : 1;
            }
            i = std::min(i + 1, text.size());
            out.append(text, start, i - start);
            continue;
        }
        if ((text.compare(i, 2, "//") == 0) || (text.compare(i, 2, "/*") == 0)) {
            auto end = nextSignificant(i);
            out.append(text, i, end - i);
            i = end;
            continue;
        }
        if (ch == ',') {
            auto next = nextSignificant(i + 1);
            if ((next < text.size()) && ((text[next] == '}') || (text[next] == ']'))) {
                i++;
                continue;
            }
        }
        out += ch;
        i++;
    }
    return out;
}

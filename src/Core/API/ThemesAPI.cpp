//
// See ThemesAPI.h
//

#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>

#include "Core/Config/Config.h"
#include "Core/Config/ConfigNode.h"
#include "Core/Theme/ThemeImporter.h"
#include "Core/XDGEnvironment.h"
#include "ConfigAPI.h"
#include "ThemesAPI.h"

using namespace gedit;

static std::optional<std::string> ReadText(const std::filesystem::path &path);
static std::filesystem::path ResolveSourcePath(const std::string &file);
static std::optional<std::string> ThemeFileName(const std::string &name, const std::string &themeName, std::string &error);
static std::string Quoted(const std::string &str);

ThemesAPI::Result ThemesAPI::Import(const std::string &file, const std::string &name, bool isForced) {
    Result result;
    auto sourcePath = ResolveSourcePath(file);
    auto text = ReadText(sourcePath);
    if (!text.has_value()) {
        result.lines.push_back("Can't read theme '" + sourcePath.string() + "'");
        return result;
    }
    // An included parent theme is relative to the theme that includes it
    auto imported = ThemeImporter::Import(*text, [&sourcePath](const std::string &includePath) {
        return ReadText(sourcePath.parent_path() / includePath);
    });
    if (!imported.isOk) {
        result.lines.push_back("Can't import '" + sourcePath.string() + "':");
        for (auto &line : imported.report) {
            result.lines.push_back("  " + line);
        }
        return result;
    }

    std::string error;
    auto fileName = ThemeFileName(name, imported.name, error);
    if (!fileName.has_value()) {
        result.lines.push_back(error);
        return result;
    }

    // Next to the user config the editor reads, else where one would be created
    ConfigAPI configApi;
    auto userConfig = configApi.ConfigFilePath(ConfigAPI::kLevel::kUser);
    auto userDir = userConfig.parent_path();
    auto themePath = userDir / (*fileName + ".theme.yml");
    auto colorsPath = userDir / (*fileName + ".colors.json");
    std::error_code ec;
    if (!isForced && (std::filesystem::exists(themePath, ec) || std::filesystem::exists(colorsPath, ec))) {
        result.lines.push_back("'" + themePath.string() + "' or '" + colorsPath.filename().string() + "' already exists");
        result.lines.push_back("  use --force to overwrite it (your changes to it are lost), or --name <other>");
        return result;
    }
    std::filesystem::create_directories(userDir, ec);

    // The font isn't part of a colour theme - keep the one the current theme uses
    ConfigNode currentTheme;
    auto font = currentTheme.LoadConfig(Config::Instance()["main"].GetStr("theme", "default.theme.yml"))
                    ? currentTheme.GetStr("font", "Andale Mono.ttf")
                    : std::string("Andale Mono.ttf");

    std::ofstream themeStream(themePath);
    themeStream << "# Imported from " << sourcePath.string() << " by goatedit's theme import.\n";
    themeStream << "# Tune the colours in " << colorsPath.filename().string() << "; importing again (--force) overwrites both files.\n";
    if (!imported.report.empty()) {
        themeStream << "#\n# What didn't carry over:\n";
        for (auto &line : imported.report) {
            themeStream << "#   - " << line << "\n";
        }
    }
    themeStream << "name: " << Quoted(imported.name) << "\n";
    themeStream << "font: " << Quoted(font) << "\n";
    themeStream << "colorfile: " << Quoted(colorsPath.filename().string()) << "\n";
    std::ofstream colorsStream(colorsPath);
    colorsStream << imported.colorsJson << "\n";
    if (!themeStream.good() || !colorsStream.good()) {
        result.lines.push_back("Failed writing '" + themePath.string() + "' / '" + colorsPath.filename().string() + "'");
        return result;
    }

    result.lines.push_back("Imported '" + imported.name + "'");
    result.lines.push_back("  wrote " + themePath.string());
    result.lines.push_back("  wrote " + colorsPath.string());
    if (!imported.report.empty()) {
        result.lines.push_back("What didn't carry over (also noted in the theme file):");
        for (auto &line : imported.report) {
            result.lines.push_back("  - " + line);
        }
    }
    result.lines.push_back("To use it, set 'main.theme: " + themePath.filename().string() + "' in " + userConfig.string());
    result.lines.push_back("  (in the editor: .cfg --user set main.theme " + themePath.filename().string() + ") and restart");
    result.isOk = true;
    return result;
}

static std::optional<std::string> ReadText(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

static std::filesystem::path ResolveSourcePath(const std::string &file) {
    std::filesystem::path path = file;
    if (file.starts_with("~/")) {
        path = XDGEnvironment::Instance().GetUserHomePath() / file.substr(2);
    }
    std::error_code ec;
    auto absPath = std::filesystem::absolute(path, ec).lexically_normal();
    return ec ? path : absPath;
}

// A name given is used as is (it only must stay a plain file name); a theme's own name is made one:
// 'Night Owl' -> 'night-owl'
static std::optional<std::string> ThemeFileName(const std::string &name, const std::string &themeName, std::string &error) {
    if (!name.empty()) {
        if ((name.find_first_of("/\\") != std::string::npos) || (name[0] == '.')) {
            error = "--name '" + name + "' must be a plain file name (no '/', not starting with '.')";
            return std::nullopt;
        }
        return name;
    }
    std::string fileName;
    for (auto ch : themeName) {
        if (std::isalnum((unsigned char)ch) || (ch == '_') || (ch == '-')) {
            fileName += (char)std::tolower((unsigned char)ch);
        } else if (!fileName.empty() && (fileName.back() != '-')) {
            fileName += '-';
        }
    }
    while (!fileName.empty() && (fileName.back() == '-')) {
        fileName.pop_back();
    }
    return fileName.empty() ? std::string("imported") : fileName;
}

static std::string Quoted(const std::string &str) {
    std::string out = "\"";
    for (auto ch : str) {
        if ((ch == '"') || (ch == '\\')) {
            out += '\\';
        }
        out += ch;
    }
    return out + "\"";
}

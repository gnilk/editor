//
// See ThemesAPI.h
//

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>

#include "Core/Config/Config.h"
#include "Core/Config/ConfigNode.h"
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/Theme/Theme.h"
#include "Core/Theme/ThemeImporter.h"
#include "Core/XDGEnvironment.h"
#include "ConfigAPI.h"
#include "ThemesAPI.h"

using namespace gedit;

static std::optional<std::string> ReadText(const std::filesystem::path &path);
static std::filesystem::path ResolveSourcePath(const std::string &file);
static std::optional<std::string> ThemeFileName(const std::string &name, const std::string &themeName, std::string &error);
static std::string Quoted(const std::string &str);
static ThemeImporter::Provenance ProvenanceAround(const std::filesystem::path &themePath);

ThemesAPI::Result ThemesAPI::List() {
    Result result;
    auto themes = Available();
    // The theme in use - not main.theme, which a start without the user config doesn't follow
    std::string current;
    if (auto theme = Editor::Instance().GetTheme()) {
        current = std::filesystem::path(theme->GetFilename()).filename().string();
    }
    size_t width = 0;
    for (auto &theme : themes) {
        width = std::max(width, theme.name.size());
    }
    for (auto &theme : themes) {
        auto line = std::string((theme.fileName == current) ? "* " : "  ");
        line += theme.name + std::string(width - theme.name.size() + 2, ' ');
        line += (theme.isUser ? "user    " : "shipped ");
        line += theme.fileName;
        if (!theme.author.empty() || !theme.license.empty()) {
            line += "  (" + (theme.author.empty() ? std::string("?") : theme.author) + ", " +
                    (theme.license.empty() ? std::string("no licence found") : theme.license) + ")";
        }
        result.lines.push_back(line);
    }
    if (themes.empty()) {
        result.lines.push_back("No themes found");
    }
    result.lines.push_back("'.theme use <name>' switches (the file name without .theme.yml, or the theme's name)");
    result.isOk = !themes.empty();
    return result;
}

ThemesAPI::Result ThemesAPI::Use(const std::string &name) {
    Result result;
    auto lower = [](std::string text) {
        for (auto &ch : text) {
            ch = (char)std::tolower((unsigned char)ch);
        }
        return text;
    };
    std::string wanted = lower(name);
    std::optional<ThemeInfo> found;
    for (auto &theme : Available()) {
        auto stem = theme.fileName.substr(0, theme.fileName.size() - std::string(".theme.yml").size());
        if ((lower(theme.fileName) == wanted) || (lower(stem) == wanted) || (lower(theme.name) == wanted)) {
            found = theme;
            break;
        }
    }
    if (!found.has_value()) {
        result.lines.push_back("No theme '" + name + "' - '.theme list' shows them");
        return result;
    }
    std::string problem;
    if (!Editor::Instance().UseTheme(found->fileName, problem)) {
        result.lines.push_back("Theme '" + found->name + "' can't be loaded: " + problem + " - keeping the current one");
        return result;
    }
    result.lines.push_back("Using '" + found->name + "'");
    auto saved = ConfigAPI().Set(ConfigAPI::kLevel::kUser, "main.theme", found->fileName);
    for (auto &line : saved.lines) {
        result.lines.push_back("  " + line);
    }
    result.isOk = true;
    return result;
}

// In lookup order (Theme::Locate): the user's themes/, the user dir, the shipped Themes/ - a file name once, where
// Locate would find it
std::vector<ThemesAPI::ThemeInfo> ThemesAPI::Available() {
    struct Folder {
        std::filesystem::path path;
        bool isUser;
    };
    std::vector<Folder> folders;
    auto userDir = ConfigAPI().ConfigFilePath(ConfigAPI::kLevel::kUser).parent_path();
    folders.push_back({userDir / Theme::USER_THEME_DIR, true});
    folders.push_back({userDir, true});
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    for (auto &dir : assetLoader.ResolveExistingDirectories(Theme::SYSTEM_THEME_DIR, AssetLoaderBase::kLocationType::kSystem)) {
        folders.push_back({dir, false});
    }

    std::vector<ThemeInfo> themes;
    for (auto &folder : folders) {
        std::error_code ec;
        if (!std::filesystem::is_directory(folder.path, ec)) {
            continue;
        }
        std::vector<std::filesystem::path> files;
        for (auto &entry : std::filesystem::directory_iterator(folder.path, ec)) {
            auto fileName = entry.path().filename().string();
            if (entry.is_regular_file(ec) && (fileName.size() > 10) && fileName.ends_with(".theme.yml")) {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());
        for (auto &path : files) {
            auto fileName = path.filename().string();
            bool isShadowed = std::any_of(themes.begin(), themes.end(), [&fileName](const ThemeInfo &theme) {
                return theme.fileName == fileName;
            });
            auto text = ReadText(path);
            if (isShadowed || !text.has_value()) {
                continue;
            }
            ThemeInfo info;
            info.fileName = fileName;
            info.isUser = folder.isUser;
            try {
                auto node = ConfigNode::FromString(*text);
                if (!node.has_value() || !node->HasKey("colorfile")) {
                    continue;       // not a theme (a stray .theme.yml)
                }
                info.name = node->GetStr("name", fileName);
                info.author = node->GetStr("author", "");
                info.license = node->GetStr("license", "");
            } catch (const std::exception &) {
                continue;
            }
            themes.push_back(info);
        }
    }
    return themes;
}

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
    }, ProvenanceAround(sourcePath));
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

    // In 'themes/' next to the user config the editor reads, else where one would be created
    ConfigAPI configApi;
    auto userConfig = configApi.ConfigFilePath(ConfigAPI::kLevel::kUser);
    auto userDir = userConfig.parent_path() / Theme::USER_THEME_DIR;
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
    auto currentLocation = Theme::Locate(Config::Instance()["main"].GetStr("theme", Theme::DEFAULT_THEME));
    auto font = (currentLocation.has_value() && currentTheme.LoadConfig(currentLocation->name, currentLocation->locationType))
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
    // Not read by the editor - where the theme comes from and its terms (docs/theme-collection.md §3)
    auto &provenance = imported.provenance;
    themeStream << "author: " << Quoted(provenance.author) << "\n";
    themeStream << "license: " << Quoted(provenance.license) << "\n";
    themeStream << "copyright: " << Quoted(provenance.copyright) << "\n";
    themeStream << "source: " << Quoted(provenance.source.empty() ? sourcePath.string() : provenance.source) << "\n";
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

// The package the theme belongs to: the nearest package.json in its folder or up to two above (a VS Code extension:
// themes/x.json next to ../package.json), and a LICENSE in that package folder - never further up, where a
// product's own licence lives (VS Code's app bundle). No package.json: the theme's own folder.
static ThemeImporter::Provenance ProvenanceAround(const std::filesystem::path &themePath) {
    ThemeImporter::Provenance provenance;
    auto packageDir = themePath.parent_path();
    auto dir = packageDir;
    for (int level = 0; level < 3; level++) {
        auto packageText = ReadText(dir / "package.json");
        if (packageText.has_value()) {
            provenance = ThemeImporter::FromPackageJson(*packageText);
            packageDir = dir;
            break;
        }
        if (!dir.has_parent_path() || (dir.parent_path() == dir)) {
            break;
        }
        dir = dir.parent_path();
    }
    for (auto name : {"LICENSE", "LICENSE.txt", "LICENSE.md"}) {
        auto licenseText = ReadText(packageDir / name);
        if (licenseText.has_value()) {
            provenance.copyright = ThemeImporter::CopyrightLine(*licenseText);
            break;
        }
    }
    return provenance;
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

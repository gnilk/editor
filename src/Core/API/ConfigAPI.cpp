//
// See ConfigAPI.h
//

#include <fstream>
#include <sstream>
#include <functional>
#include <yaml-cpp/yaml.h>

#include "Core/Config/Config.h"
#include "Core/Config/ConfigFilePatcher.h"
#include "Core/Config/ProjectConfigPolicy.h"
#include "Core/RuntimeConfig.h"
#include "Core/StrUtil.h"
#include "Core/Editor.h"
#include "EditorAPI.h"
#include "ConfigAPI.h"

using namespace gedit;

static const char *configFileName = "config.yml";

namespace {
    // The level files, loaded fresh for one call
    struct LevelFiles {
        YAML::Node system;
        YAML::Node user;
        YAML::Node project;
        bool isUserApplied = false;      // merged into the running config (not --skip-user-config)
        bool isProjectApplied = false;
    };
    struct Entry {
        std::string key;
        std::string value;
    };
}

static LevelFiles LoadLevelFiles(ConfigAPI &configApi);
static YAML::Node LoadFile(const std::filesystem::path &path);
static std::string LevelName(ConfigAPI::kLevel level);
static std::string OriginOf(const LevelFiles &files, const std::vector<std::string> &keyPath);
static std::optional<std::string> EffectiveValue(const LevelFiles &files, const std::vector<std::string> &keyPath);
static std::vector<Entry> Flatten(const YAML::Node &root);
static std::string ValueToString(const YAML::Node &node);
static bool ReadFile(const std::filesystem::path &path, std::string &outText);
static bool WriteFileAtomic(const std::filesystem::path &path, const std::string &text, std::string &outError);

std::optional<ConfigAPI::kLevel> ConfigAPI::LevelFromString(const std::string &level) {
    if (level.empty()) {
        return kLevel::kRunning;
    }
    if (level == "user") {
        return kLevel::kUser;
    }
    if (level == "project") {
        return kLevel::kProject;
    }
    return {};
}

std::vector<std::string> ConfigAPI::SplitKey(const std::string &key) {
    std::vector<std::string> keyPath;
    std::string segment;
    for (auto ch : key) {
        if (ch == '.') {
            if (segment.empty()) {
                return {};
            }
            keyPath.push_back(segment);
            segment.clear();
            continue;
        }
        segment += ch;
    }
    if (segment.empty()) {
        return {};
    }
    keyPath.push_back(segment);
    return keyPath;
}

ConfigAPI::Result ConfigAPI::List(kLevel level, const std::string &prefix) {
    Result result;
    YAML::Node root;
    LevelFiles files;
    if (level == kLevel::kRunning) {
        root = Config::Instance().GetDataNode();
        files = LoadLevelFiles(*this);
    } else {
        auto path = ConfigFilePath(level);
        if (path.empty()) {
            result.lines.push_back("No project is open");
            return result;
        }
        if (!std::filesystem::exists(path)) {
            result.lines.push_back("No " + LevelName(level) + " config (" + path.string() + ")");
            result.isOk = true;
            return result;
        }
        root = LoadFile(path);
        result.lines.push_back("# " + path.string());
    }
    for (auto &entry : Flatten(root)) {
        if (!strutil::startsWith(entry.key, prefix)) {
            continue;
        }
        auto line = entry.key + " = " + entry.value;
        if (level == kLevel::kRunning) {
            line += "  [" + OriginOf(files, SplitKey(entry.key)) + "]";
        }
        result.lines.push_back(line);
    }
    result.isOk = true;
    return result;
}

ConfigAPI::Result ConfigAPI::Get(kLevel level, const std::string &key) {
    Result result;
    auto keyPath = SplitKey(key);
    if (keyPath.empty()) {
        result.lines.push_back("Invalid key '" + key + "' (use section.key)");
        return result;
    }
    if (level == kLevel::kRunning) {
        auto node = ConfigNode::NodeAtPath(Config::Instance().GetDataNode(), keyPath);
        if (!node.IsDefined()) {
            result.lines.push_back(key + " is not set");
            return result;
        }
        auto files = LoadLevelFiles(*this);
        result.lines.push_back(key + " = " + ValueToString(node) + "  [" + OriginOf(files, keyPath) + "]");
        result.isOk = true;
        return result;
    }
    auto path = ConfigFilePath(level);
    if (path.empty()) {
        result.lines.push_back("No project is open");
        return result;
    }
    auto node = ConfigNode::NodeAtPath(LoadFile(path), keyPath);
    if (!node.IsDefined()) {
        result.lines.push_back(key + " is not set in the " + LevelName(level) + " config");
        return result;
    }
    result.lines.push_back(key + " = " + ValueToString(node));
    result.isOk = true;
    return result;
}

ConfigAPI::Result ConfigAPI::Set(kLevel level, const std::string &key, const std::string &value) {
    Result result;
    if (level == kLevel::kRunning) {
        result.lines.push_back("set needs a level: --user or --project");
        return result;
    }
    auto keyPath = SplitKey(key);
    if (keyPath.size() < 2) {
        result.lines.push_back("Invalid key '" + key + "' (use section.key)");
        return result;
    }
    if ((level == kLevel::kProject) && ProjectConfigPolicy::IsDenied(keyPath)) {
        result.lines.push_back(key + " can't be set in a project config (it runs commands/code or picks a write path)");
        return result;
    }
    auto running = ConfigNode::NodeAtPath(Config::Instance().GetDataNode(), keyPath);
    if (running.IsDefined() && (running.IsMap() || running.IsSequence())) {
        result.lines.push_back(key + " is a section or a list - use '.cfg --" + LevelName(level) + " edit'");
        return result;
    }
    auto path = ConfigFilePath(level);
    if (path.empty()) {
        result.lines.push_back("No project is open");
        return result;
    }

    std::string text;
    if (std::filesystem::exists(path) && !ReadFile(path, text)) {
        result.lines.push_back("Unable to read " + path.string());
        return result;
    }
    auto patched = ConfigFilePatcher::SetScalar(text, keyPath, value);
    if (!patched.isOk) {
        result.lines.push_back("Can't edit " + path.string() + " safely: " + patched.error);
        result.lines.push_back("Nothing was written - use '.cfg --" + LevelName(level) + " edit' to change it by hand");
        return result;
    }
    std::string error;
    if (!WriteFileAtomic(path, patched.text, error)) {
        result.lines.push_back("Unable to write " + path.string() + ": " + error);
        return result;
    }
    result.isOk = true;
    result.lines.push_back(key + " = " + value + " written to " + path.string());
    if (!running.IsDefined()) {
        result.lines.push_back("Note: " + key + " is not in the running config - check the spelling");
    }

    // Update the running instance with the EFFECTIVE value - which may not be the one just written
    auto files = LoadLevelFiles(*this);
    bool isLevelApplied = (level == kLevel::kUser) ? files.isUserApplied : files.isProjectApplied;
    if (!isLevelApplied) {
        result.lines.push_back("Note: the running editor doesn't use the " + LevelName(level) + " config (--skip-user-config or no project at startup) - unchanged");
        return result;
    }
    if ((level == kLevel::kUser) && files.isProjectApplied && !ProjectConfigPolicy::IsDenied(keyPath)) {
        auto projectNode = ConfigNode::NodeAtPath(files.project, keyPath);
        if (projectNode.IsDefined()) {
            result.lines.push_back("Warning: the project config overrides it (" + ValueToString(projectNode) + ") - the running value is unchanged");
        }
    }
    auto effective = EffectiveValue(files, keyPath);
    if (effective.has_value()) {
        // 'running' points into the live tree - take the old value before it's overwritten
        auto oldValue = running.IsDefined() ? std::optional<std::string>(ValueToString(running)) : std::nullopt;
        try {
            Config::Instance().SetScalarAtPath(keyPath, effective.value());
            if (oldValue != effective) {
                ReInitializeViews();
            }
        } catch (const YAML::Exception &e) {
            result.lines.push_back(std::string("Note: couldn't update the running config: ") + e.what());
        }
    }
    result.lines.push_back("Applied to the views - settings only read at startup need a restart");
    return result;
}

// Views re-read their config in ReInitView - re-initialize the whole tree, the same pair the view resize
// actions use. Posted, so it runs from the runloop once the script (and the command line commit) is done.
void ConfigAPI::ReInitializeViews() {
    if (!RuntimeConfig::Instance().HasRootView()) {
        return;
    }
    auto &rootView = RuntimeConfig::Instance().GetRootView();
    rootView.PostMessage([&rootView]() {
        rootView.Initialize();
        rootView.InvalidateAll();
    });
}

ConfigAPI::Result ConfigAPI::Edit(kLevel level) {
    Result result;
    if (level == kLevel::kRunning) {
        result.lines.push_back("edit needs a level: --user or --project");
        return result;
    }
    auto path = ConfigFilePath(level);
    if (path.empty()) {
        result.lines.push_back("No project is open");
        return result;
    }
    if (!std::filesystem::exists(path)) {
        std::string header = "# GoatEdit " + LevelName(level) + " config - merged over the ";
        header += (level == kLevel::kUser) ? "system config at startup\n" : "system and user config at startup\n";
        if (level == kLevel::kProject) {
            header += "# Ignored here: main.bootstrap_scripts, main.plugin_directory, terminal.shell/init/bootstrap, logging.logfile\n";
        }
        std::string error;
        if (!WriteFileAtomic(path, header, error)) {
            result.lines.push_back("Unable to create " + path.string() + ": " + error);
            return result;
        }
    }
    auto editorApi = Editor::Instance().GetGlobalAPIObject<EditorAPI>();
    if ((editorApi == nullptr) || (editorApi->LoadDocument(path.c_str()) == nullptr)) {
        result.lines.push_back("Unable to open " + path.string());
        return result;
    }
    result.isOk = true;
    result.lines.push_back("Opened " + path.string() + " - changes apply on restart");
    return result;
}

std::filesystem::path ConfigAPI::ConfigFilePath(kLevel level) {
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    if (level == kLevel::kUser) {
        auto existing = assetLoader.ResolveExistingPath(configFileName, AssetLoaderBase::kLocationType::kUser);
        if (!existing.empty()) {
            return existing;
        }
        return Editor::GetDefaultUserConfigDir() / configFileName;
    }
    if (level == kLevel::kProject) {
        return assetLoader.ResolveWritePath(configFileName, AssetLoaderBase::kLocationType::kProject);
    }
    return {};
}

//
// Helpers
//
static LevelFiles LoadLevelFiles(ConfigAPI &configApi) {
    LevelFiles files;
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    files.system = LoadFile(assetLoader.ResolveExistingPath(configFileName, AssetLoaderBase::kLocationType::kSystem));
    files.user = LoadFile(configApi.ConfigFilePath(ConfigAPI::kLevel::kUser));
    auto projectPath = configApi.ConfigFilePath(ConfigAPI::kLevel::kProject);
    files.project = LoadFile(projectPath);
    files.isUserApplied = Editor::Instance().IsUserConfigEnabled();
    files.isProjectApplied = files.isUserApplied && !projectPath.empty();
    return files;
}

// Missing/unparseable file -> an empty map, so lookups just come back undefined
static YAML::Node LoadFile(const std::filesystem::path &path) {
    std::error_code ec;
    if (path.empty() || !std::filesystem::is_regular_file(path, ec)) {
        return YAML::Node(YAML::NodeType::Map);
    }
    try {
        auto node = YAML::LoadFile(path.string());
        if (node.IsMap()) {
            return node;
        }
    } catch (const YAML::Exception &) {
    }
    return YAML::Node(YAML::NodeType::Map);
}

static std::string LevelName(ConfigAPI::kLevel level) {
    switch (level) {
        case ConfigAPI::kLevel::kUser :
            return "user";
        case ConfigAPI::kLevel::kProject :
            return "project";
        default:
            return "running";
    }
}

// Highest level that defines the key: project (unless denied there) > user > system
static std::string OriginOf(const LevelFiles &files, const std::vector<std::string> &keyPath) {
    if (files.isProjectApplied && !ProjectConfigPolicy::IsDenied(keyPath) && ConfigNode::NodeAtPath(files.project, keyPath).IsDefined()) {
        return "project";
    }
    if (files.isUserApplied && ConfigNode::NodeAtPath(files.user, keyPath).IsDefined()) {
        return "user";
    }
    if (ConfigNode::NodeAtPath(files.system, keyPath).IsDefined()) {
        return "system";
    }
    return "runtime";
}

static std::optional<std::string> EffectiveValue(const LevelFiles &files, const std::vector<std::string> &keyPath) {
    std::vector<std::pair<bool, const YAML::Node *>> levels = {
        { files.isProjectApplied && !ProjectConfigPolicy::IsDenied(keyPath), &files.project },
        { files.isUserApplied, &files.user },
        { true, &files.system },
    };
    for (auto &[isApplied, root] : levels) {
        if (!isApplied) {
            continue;
        }
        auto node = ConfigNode::NodeAtPath(*root, keyPath);
        if (node.IsDefined() && node.IsScalar()) {
            return node.Scalar();
        }
    }
    return {};
}

static std::vector<Entry> Flatten(const YAML::Node &root) {
    std::vector<Entry> entries;
    std::function<void(const YAML::Node &, const std::string &)> walk = [&](const YAML::Node &node, const std::string &prefix) {
        if (node.IsMap()) {
            for (auto it : node) {
                auto key = it.first.Scalar();
                walk(it.second, prefix.empty() ? key : prefix + "." + key);
            }
            return;
        }
        entries.push_back({prefix, ValueToString(node)});
    };
    walk(root, "");
    return entries;
}

static std::string ValueToString(const YAML::Node &node) {
    if (node.IsScalar()) {
        return node.Scalar();
    }
    if (node.IsNull()) {
        return "~";
    }
    YAML::Emitter emitter;
    emitter << YAML::Flow << node;
    return emitter.c_str();
}

static bool ReadFile(const std::filesystem::path &path, std::string &outText) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    outText = buffer.str();
    return true;
}

// tmp -> rename, so a crash mid-write never leaves a half-written config. A symlinked config (dotfiles)
// is written through to its target - renaming over the link would replace it with a plain file.
static bool WriteFileAtomic(const std::filesystem::path &linkOrPath, const std::string &text, std::string &outError) {
    std::error_code ec;
    auto path = linkOrPath;
    if (std::filesystem::is_symlink(linkOrPath, ec)) {
        path = std::filesystem::canonical(linkOrPath, ec);
        if (ec) {
            outError = "can't resolve the symlink: " + ec.message();
            return false;
        }
    }
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        outError = ec.message();
        return false;
    }
    auto tmpPath = path;
    tmpPath += ".tmp";
    {
        std::ofstream file(tmpPath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            outError = "can't create " + tmpPath.string();
            return false;
        }
        file << text;
        file.flush();
        if (!file.good()) {
            outError = "write failed";
            return false;
        }
    }
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        outError = "rename failed";
        return false;
    }
    return true;
}

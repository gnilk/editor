//
// UserDirs - the table in UserDirs.h. Platform independent: the platform is a parameter.
//

#include <mutex>

#include "UserDirs.h"
#include "Core/Platform.h"
#include "Core/StrUtil.h"

using namespace gedit;

// Lower case on every platform but Windows' own app folders, where the app name is the custom
static const char *glbDirName = "goatedit";
static const char *glbAppName = "GoatEdit";

static std::string EnvOrEmpty(const UserDirs::EnvLookup &env, const std::string &name) {
    auto value = env(name);
    return value.has_value() ? *value : std::string();
}

// An XDG base dir: the variable when absolute, else the home-relative default
static std::filesystem::path XDGDir(const UserDirs::EnvLookup &env, const std::string &name, const std::filesystem::path &home,
                                    const std::filesystem::path &homeRelativeDefault) {
    auto value = EnvOrEmpty(env, name);
    if (!value.empty() && (value[0] == '/')) {
        return value;
    }
    if (home.empty()) {
        return {};
    }
    return home / homeRelativeDefault;
}

static std::vector<std::filesystem::path> XDGDirList(const UserDirs::EnvLookup &env, const std::string &name,
                                                     const std::string &defaultValue) {
    auto value = env(name);
    std::vector<std::string> parts;
    strutil::split(parts, value.has_value() ? *value : defaultValue, ':');
    std::vector<std::filesystem::path> dirs;
    for (auto &part : parts) {
        if (!part.empty()) {
            dirs.emplace_back(part);
        }
    }
    return dirs;
}

static std::filesystem::path GitGlobalIgnoreFile(const UserDirs::EnvLookup &env, const std::filesystem::path &home) {
    // As git reads it: $XDG_CONFIG_HOME when set at all, else ~/.config
    if (auto configHome = EnvOrEmpty(env, "XDG_CONFIG_HOME"); !configHome.empty()) {
        return std::filesystem::path(configHome) / "git" / "ignore";
    }
    if (home.empty()) {
        return {};
    }
    return home / ".config" / "git" / "ignore";
}

static UserDirs ResolveUnix(UserDirs::kPlatform platform, const UserDirs::EnvLookup &env) {
    UserDirs dirs;
    dirs.home = EnvOrEmpty(env, "HOME");
    if (!dirs.home.empty()) {
        dirs.assetDirs = {
            XDGDir(env, "XDG_CONFIG_HOME", dirs.home, ".config") / glbDirName,      // preferred
            dirs.home / ".goatedit",                                                // legacy
        };
        dirs.stateDir = XDGDir(env, "XDG_STATE_HOME", dirs.home, std::filesystem::path(".local") / "state") / glbDirName;
        if (platform == UserDirs::kPlatform::kMacOS) {
            dirs.logDir = dirs.home / "Library" / "Logs" / glbAppName;
        } else {
            dirs.logDir = dirs.stateDir;
        }
    }
    dirs.systemDataDirs = XDGDirList(env, "XDG_DATA_DIRS", "/usr/local/share:/usr/share");
    dirs.gitGlobalIgnoreFile = GitGlobalIgnoreFile(env, dirs.home);
    return dirs;
}

static UserDirs ResolveWindows(const UserDirs::EnvLookup &env) {
    UserDirs dirs;
    // %USERPROFILE%, not %HOME% - Git Bash sets HOME, a plain cmd doesn't; USERPROFILE is always there
    dirs.home = EnvOrEmpty(env, "USERPROFILE");
    if (dirs.home.empty()) {
        auto drive = EnvOrEmpty(env, "HOMEDRIVE");
        auto path = EnvOrEmpty(env, "HOMEPATH");
        if (!drive.empty() && !path.empty()) {
            dirs.home = drive + path;
        }
    }
    std::filesystem::path localAppData = EnvOrEmpty(env, "LOCALAPPDATA");
    if (localAppData.empty() && !dirs.home.empty()) {
        localAppData = dirs.home / "AppData" / "Local";
    }
    if (!dirs.home.empty()) {
        dirs.assetDirs = { dirs.home / ".goatedit" };
    }
    if (!localAppData.empty()) {
        dirs.stateDir = localAppData / glbAppName;
        dirs.logDir = localAppData / glbAppName / "Logs";
    }
    dirs.gitGlobalIgnoreFile = GitGlobalIgnoreFile(env, dirs.home);
    return dirs;
}

UserDirs UserDirs::Resolve(kPlatform platform, const EnvLookup &env) {
    if (platform == kPlatform::kWindows) {
        return ResolveWindows(env);
    }
    return ResolveUnix(platform, env);
}

static std::mutex glbCurrentLock;
static std::optional<UserDirs> glbCurrent;

const UserDirs &UserDirs::Current() {
    std::lock_guard<std::mutex> guard(glbCurrentLock);
    if (!glbCurrent.has_value()) {
        glbCurrent = Resolve(CurrentPlatform(), [](const std::string &name) {
            return Platform::GetEnv(name);
        });
    }
    return *glbCurrent;
}

void UserDirs::Reset() {
    std::lock_guard<std::mutex> guard(glbCurrentLock);
    glbCurrent.reset();
}

UserDirs::kPlatform UserDirs::CurrentPlatform() {
    auto name = Platform::Name();
    if (name == "windows") {
        return kPlatform::kWindows;
    }
    if (name == "macos") {
        return kPlatform::kMacOS;
    }
    return kPlatform::kLinux;
}

const std::filesystem::path &UserDirs::Home() {
    return Current().home;
}

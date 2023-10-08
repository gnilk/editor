//
// Created by gnilk on 07.08.23.
//
// Implementation of the XDG path specification variables...
// see: https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
//

#include "Core/StrUtil.h"
#include "XDGEnvironment.h"

using namespace gedit;

static const std::string ENV_XDG_DATA_HOME = "XDG_DATA_HOME";
static const std::string ENV_XDG_STATE_HOME = "XDG_STATE_HOME";
static const std::string ENV_XDG_CONFIG_HOME = "XDG_CONFIG_HOME";
static const std::string ENV_XDG_CACHE_HOME = "XDG_CACHE_HOME";
static const std::string ENV_XDG_RUNTIME_DIR = "XDG_RUNTIME_DIR";
static const std::string ENV_XDG_SESSION_TYPE = "XDG_SESSION_TYPE";
static const std::string ENV_XDG_CONFIG_DIRS = "XDG_CONFIG_DIRS";
static const std::string ENV_XDG_DATA_DIRS = "XDG_DATA_DIRS";

// Returns the global instance - yes we are a singleton - makes sense in this case...
const XDGEnvironment &XDGEnvironment::Instance() {
    static XDGEnvironment glbInstance;
    glbInstance.Initialize();
    return glbInstance;
}
// Reset and re-initialize...
void XDGEnvironment::Reset() {
    isInitialized = false;
    Initialize();
}

const std::string &XDGEnvironment::GetSessionType() const {
    return sessionType;
}

bool XDGEnvironment::IsTerminalSession() const {
#ifdef GEDIT_MACOS
    return false;   // Don't support headless on macos
#elif defined(GEDIT_LINUX)
    // Should this have a list???
    if(sessionType == "tty") {
        return true;
    }
    return false;
#endif
}


const std::filesystem::path &XDGEnvironment::GetUserHomePath() const {
    return pathHome;
}

const std::filesystem::path &XDGEnvironment::GetUserDataPath() const {
    // Not sure if we should auto create here...
    return userData.fsPath;
}
const std::filesystem::path &XDGEnvironment::GetUserConfigPath() const {
    // Do not auto-create this
    return userConfig.fsPath;
}
const std::filesystem::path &XDGEnvironment::GetUserCachePath() const {
    // We fetch this - make sure it exists...
    if (!exists(userCache.fsPath)) {
        std::filesystem::create_directories(userCache.fsPath);
    }
    return userCache.fsPath;
}
const std::filesystem::path &XDGEnvironment::GetUserStatePath() const {
    // We fetch this - make sure it exists...
    if (!exists(userState.fsPath)) {
        std::filesystem::create_directories(userState.fsPath);
    }
    return userState.fsPath;
}
const std::filesystem::path &XDGEnvironment::GetUserRuntimePath() const {
    return userRuntime.fsPath;
}
const std::vector<std::filesystem::path> &XDGEnvironment::GetSystemConfigPaths() const {
    return sysConfig.fsPaths;
}
const std::vector<std::filesystem::path> &XDGEnvironment::GetSystemDataPaths() const {
    return sysData.fsPaths;
}
const std::filesystem::path &XDGEnvironment::GetFirstSystemDataPath() const {
    return sysData.fsPaths.front();
}
const std::filesystem::path &XDGEnvironment::GetFirstSystemConfigPath() const {
    return sysConfig.fsPaths.front();
}

// Helper to return the first with a specific prefix from a list/vector of paths...
static std::optional<const std::filesystem::path>GetFirstWithPrefix(const std::vector<std::filesystem::path> &pathList, const std::string &prefix) {
    for(auto &path : pathList) {
        if (strutil::startsWith(path, prefix)) {
            return {path};
        }
    }
    return {};

}
std::optional<const std::filesystem::path>XDGEnvironment::GetFirstSystemDataPathWithPrefix(const std::string &prefix) const {
    return GetFirstWithPrefix(sysData.fsPaths, prefix);
}

std::optional<const std::filesystem::path> XDGEnvironment::GetFirstSystemDataPathWithSubDir(const std::string &subdir) const {
    for(auto &sysPath : sysData.fsPaths) {
        auto sysSubPath = sysPath / subdir;
        if (std::filesystem::is_directory(sysSubPath)) {
            return sysSubPath;
        }
    }
    return {};
}

std::optional<const std::filesystem::path>XDGEnvironment::GetFirstSystemConfigPathWithPrefix(const std::string &prefix) const {
    return GetFirstWithPrefix(sysConfig.fsPaths, prefix);
}

//
// Internal helpers...
//
bool XDGEnvironment::Initialize() {
    // Already initialized, skip this..
    if (isInitialized) {
        return true;
    }

    const char* home = getenv("HOME");
    if (home == nullptr) {
        return false;
    }

    auto [currentSessionType, bIsDefault] = GetEnv(ENV_XDG_SESSION_TYPE,"tty");
    sessionType = currentSessionType;


    // Home is treated differently
    userHome = std::string(home);
    pathHome = std::filesystem::path(userHome);

    // Resolve lists
    ResolvePathListFromVar(sysConfig, ENV_XDG_CONFIG_DIRS, "/etc/xdg");
    ResolvePathListFromVar(sysData, ENV_XDG_DATA_DIRS, "/usr/local/share:/usr/share");

    // Resolve the XDG paths
    ResolvePathAndVar(userData, ENV_XDG_DATA_HOME, "/.local/share");
    ResolvePathAndVar(userState, ENV_XDG_STATE_HOME, "/.local/state");
    ResolvePathAndVar(userConfig, ENV_XDG_CONFIG_HOME, "/.config");
    ResolvePathAndVar(userCache, ENV_XDG_CACHE_HOME, "/.cache");
    ResolvePathAndVar(userRuntime, ENV_XDG_RUNTIME_DIR, "");


    isInitialized = true;


    return isInitialized;
}


bool XDGEnvironment::ResolvePathAndVar(XDGEnvironment::PathVar &pathVar, const std::string &var, const std::string &defaultValue) {
    std::tie(pathVar.envValue, pathVar.isDefault) = GetEnv(var, defaultValue);
    pathVar.fsPath = PathFromVar(pathVar.envValue);
    return true;
}
bool XDGEnvironment::ResolvePathListFromVar(XDGEnvironment::PathList &pathList, const std::string &var, const std::string &defaultValue) {
    std::tie(pathList.envValue, pathList.isDefault) = GetEnv(var, defaultValue);
    return PathListFromVar(pathList.fsPaths, pathList.envValue);
}

std::pair<std::string, bool> XDGEnvironment::GetEnv(const std::string &var, const std::string &defaultValue) {
    const char* temp = getenv(var.c_str());
    if (temp != nullptr) {
        return {temp, defaultValue == temp};
    }
    return {defaultValue, true};
}

std::filesystem::path XDGEnvironment::PathFromVar(const std::string &var) {
    if (var.empty()) {
        return {};
    }
    if (var.at(0) != '/') {
        return {var};
    }
    // Should be relative home dir - strip of root of config var (otherwise append will only return the var and not the concat)
    return pathHome / var.substr(1);
}

bool XDGEnvironment::PathListFromVar(std::vector<std::filesystem::path> &outList, const std::string &varValue) {
    std::vector<std::string> parts;
    strutil::split(parts, varValue, ':');
    if (parts.empty()) {
        return false;
    }

    // Clear if we already have something..
    outList.clear();
    for(auto &varPart : parts) {
        outList.emplace_back(varPart);
    }
    return true;
}


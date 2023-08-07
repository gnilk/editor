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

const std::filesystem::path &XDGEnvironment::GetUserHomePath() const {
    return pathHome;
}

const std::filesystem::path &XDGEnvironment::GetUserDataPath() const {
    return userData.fsPath;
}
const std::filesystem::path &XDGEnvironment::GetUserConfigPath() const {
    return userConfig.fsPath;
}
const std::filesystem::path &XDGEnvironment::GetUserCachePath() const {
    return userCache.fsPath;
}
const std::filesystem::path &XDGEnvironment::GetUserStatePath() const {
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


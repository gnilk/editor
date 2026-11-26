//
// UserDirs (src/Core/UserDirs.h) - the user's dirs per platform, from an environment passed in. Every platform's table
// is tested here on whichever OS runs the tests; the environment is a map, never the real one.
//
#include <testinterface.h>

#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include "Core/UserDirs.h"
#include "Core/Platform.h"

using namespace gedit;
using kPlatform = UserDirs::kPlatform;
namespace fs = std::filesystem;

extern "C" {
    DLL_EXPORT int test_userdirs(ITesting *t);
    DLL_EXPORT int test_userdirs_linux(ITesting *t);
    DLL_EXPORT int test_userdirs_macos(ITesting *t);
    DLL_EXPORT int test_userdirs_xdg(ITesting *t);
    DLL_EXPORT int test_userdirs_xdginvalid(ITesting *t);
    DLL_EXPORT int test_userdirs_nohome(ITesting *t);
    DLL_EXPORT int test_userdirs_gitignore(ITesting *t);
    DLL_EXPORT int test_userdirs_windows(ITesting *t);
    DLL_EXPORT int test_userdirs_windowsfallback(ITesting *t);
    DLL_EXPORT int test_userdirs_current(ITesting *t);
}

static UserDirs::EnvLookup Env(std::map<std::string, std::string> vars) {
    return [vars = std::move(vars)](const std::string &name) -> std::optional<std::string> {
        auto it = vars.find(name);
        if (it == vars.end()) {
            return std::nullopt;
        }
        return it->second;
    };
}

DLL_EXPORT int test_userdirs(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_userdirs_linux(ITesting *t) {
    auto dirs = UserDirs::Resolve(kPlatform::kLinux, Env({{"HOME", "/home/goat"}}));
    TR_ASSERT(t, dirs.home == "/home/goat");
    // The XDG config dir is preferred (it's where new files are written); the legacy dir follows
    TR_ASSERT(t, dirs.assetDirs.size() == 2);
    TR_ASSERT(t, dirs.assetDirs[0] == "/home/goat/.config/goatedit");
    TR_ASSERT(t, dirs.assetDirs[1] == "/home/goat/.goatedit");
    TR_ASSERT(t, dirs.stateDir == "/home/goat/.local/state/goatedit");
    TR_ASSERT(t, dirs.logDir == dirs.stateDir);
    // XDG_DATA_DIRS unset: the spec's default
    TR_ASSERT(t, dirs.systemDataDirs.size() == 2);
    TR_ASSERT(t, dirs.systemDataDirs[0] == "/usr/local/share");
    TR_ASSERT(t, dirs.systemDataDirs[1] == "/usr/share");
    // Never '$XDG_DATA_HOME/goatedit': a per-user install puts its system assets there
    for (auto &dir : dirs.assetDirs) {
        TR_ASSERT(t, dir != "/home/goat/.local/share/goatedit");
    }
    return kTR_Pass;
}

DLL_EXPORT int test_userdirs_macos(ITesting *t) {
    auto dirs = UserDirs::Resolve(kPlatform::kMacOS, Env({{"HOME", "/Users/goat"}}));
    TR_ASSERT(t, dirs.home == "/Users/goat");
    // The same config dirs as Linux (not ~/Library/Application Support)
    TR_ASSERT(t, dirs.assetDirs.size() == 2);
    TR_ASSERT(t, dirs.assetDirs[0] == "/Users/goat/.config/goatedit");
    TR_ASSERT(t, dirs.assetDirs[1] == "/Users/goat/.goatedit");
    TR_ASSERT(t, dirs.stateDir == "/Users/goat/.local/state/goatedit");
    // Logs where Console.app looks
    TR_ASSERT(t, dirs.logDir == "/Users/goat/Library/Logs/GoatEdit");
    return kTR_Pass;
}

DLL_EXPORT int test_userdirs_xdg(ITesting *t) {
    auto dirs = UserDirs::Resolve(kPlatform::kLinux, Env({
        {"HOME", "/home/goat"},
        {"XDG_CONFIG_HOME", "/xdg/config"},
        {"XDG_STATE_HOME", "/xdg/state"},
        {"XDG_DATA_DIRS", "/opt/share:/usr/share"},
    }));
    TR_ASSERT(t, dirs.assetDirs[0] == "/xdg/config/goatedit");
    // The legacy dir stays in home
    TR_ASSERT(t, dirs.assetDirs[1] == "/home/goat/.goatedit");
    TR_ASSERT(t, dirs.stateDir == "/xdg/state/goatedit");
    TR_ASSERT(t, dirs.logDir == "/xdg/state/goatedit");
    TR_ASSERT(t, dirs.systemDataDirs.size() == 2);
    TR_ASSERT(t, dirs.systemDataDirs[0] == "/opt/share");
    return kTR_Pass;
}

// A relative or empty XDG value is invalid (the spec): the default is used
DLL_EXPORT int test_userdirs_xdginvalid(ITesting *t) {
    auto dirs = UserDirs::Resolve(kPlatform::kLinux, Env({
        {"HOME", "/home/goat"},
        {"XDG_CONFIG_HOME", "relative/config"},
        {"XDG_STATE_HOME", ""},
    }));
    TR_ASSERT(t, dirs.assetDirs[0] == "/home/goat/.config/goatedit");
    TR_ASSERT(t, dirs.stateDir == "/home/goat/.local/state/goatedit");
    return kTR_Pass;
}

// No home: no user dirs at all, rather than ones relative to the cwd
DLL_EXPORT int test_userdirs_nohome(ITesting *t) {
    for (auto platform : {kPlatform::kLinux, kPlatform::kMacOS, kPlatform::kWindows}) {
        auto dirs = UserDirs::Resolve(platform, Env({{"HOME", ""}}));
        TR_ASSERT(t, dirs.home.empty());
        TR_ASSERT(t, dirs.assetDirs.empty());
        TR_ASSERT(t, dirs.stateDir.empty());
        TR_ASSERT(t, dirs.logDir.empty());
        TR_ASSERT(t, dirs.gitGlobalIgnoreFile.empty());
    }
    return kTR_Pass;
}

// As git reads it: XDG_CONFIG_HOME when set at all (git doesn't require it absolute), else ~/.config
DLL_EXPORT int test_userdirs_gitignore(ITesting *t) {
    auto dirs = UserDirs::Resolve(kPlatform::kLinux, Env({{"HOME", "/home/goat"}}));
    TR_ASSERT(t, dirs.gitGlobalIgnoreFile == "/home/goat/.config/git/ignore");
    dirs = UserDirs::Resolve(kPlatform::kLinux, Env({{"HOME", "/home/goat"}, {"XDG_CONFIG_HOME", "/xdg"}}));
    TR_ASSERT(t, dirs.gitGlobalIgnoreFile == "/xdg/git/ignore");
    dirs = UserDirs::Resolve(kPlatform::kWindows, Env({{"USERPROFILE", R"(C:\Users\goat)"}}));
    TR_ASSERT(t, dirs.gitGlobalIgnoreFile == fs::path(R"(C:\Users\goat)") / ".config" / "git" / "ignore");
    return kTR_Pass;
}

// Paths are built with fs::path's '/', so on a unix host a Windows path joins as 'C:\Users\goat/.goatedit' - the
// expected values are built the same way; what is tested is which variable each dir comes from
DLL_EXPORT int test_userdirs_windows(ITesting *t) {
    fs::path home = R"(C:\Users\goat)";
    fs::path local = R"(C:\Users\goat\AppData\Local)";
    auto dirs = UserDirs::Resolve(kPlatform::kWindows, Env({
        {"USERPROFILE", home.string()},
        {"LOCALAPPDATA", local.string()},
        // Git Bash sets HOME; it is not read on Windows, nor are the XDG dirs
        {"HOME", "/c/Users/goat"},
        {"XDG_CONFIG_HOME", "/xdg/config"},
        {"XDG_DATA_DIRS", "/usr/share"},
    }));
    TR_ASSERT(t, dirs.home == home);
    // The primary user dir: one, no legacy dir (so no "move your files" warning)
    TR_ASSERT(t, dirs.assetDirs.size() == 1);
    TR_ASSERT(t, dirs.assetDirs[0] == home / ".goatedit");
    TR_ASSERT(t, dirs.stateDir == local / "GoatEdit");
    TR_ASSERT(t, dirs.logDir == local / "GoatEdit" / "Logs");
    // Assets sit next to the exe; there is no system data dir to search
    TR_ASSERT(t, dirs.systemDataDirs.empty());
    return kTR_Pass;
}

DLL_EXPORT int test_userdirs_windowsfallback(ITesting *t) {
    // No USERPROFILE: HOMEDRIVE + HOMEPATH; no LOCALAPPDATA: home\AppData\Local
    auto dirs = UserDirs::Resolve(kPlatform::kWindows, Env({{"HOMEDRIVE", "D:"}, {"HOMEPATH", R"(\Users\goat)"}}));
    fs::path home = R"(D:\Users\goat)";
    TR_ASSERT(t, dirs.home == home);
    TR_ASSERT(t, dirs.stateDir == home / "AppData" / "Local" / "GoatEdit");
    TR_ASSERT(t, dirs.logDir == home / "AppData" / "Local" / "GoatEdit" / "Logs");
    // Half of it is no home
    dirs = UserDirs::Resolve(kPlatform::kWindows, Env({{"HOMEDRIVE", "D:"}}));
    TR_ASSERT(t, dirs.home.empty());
    return kTR_Pass;
}

// This process's: the build's platform and the real environment
DLL_EXPORT int test_userdirs_current(ITesting *t) {
    auto &current = UserDirs::Current();
    auto expected = UserDirs::Resolve(UserDirs::CurrentPlatform(), [](const std::string &name) {
        return Platform::GetEnv(name);
    });
    TR_ASSERT(t, current.home == expected.home);
    TR_ASSERT(t, current.assetDirs == expected.assetDirs);
    TR_ASSERT(t, current.logDir == expected.logDir);
    TR_ASSERT(t, &UserDirs::Home() == &current.home);
#if defined(GEDIT_MACOS)
    TR_ASSERT(t, UserDirs::CurrentPlatform() == kPlatform::kMacOS);
#elif defined(GEDIT_LINUX)
    TR_ASSERT(t, UserDirs::CurrentPlatform() == kPlatform::kLinux);
#endif
    return kTR_Pass;
}

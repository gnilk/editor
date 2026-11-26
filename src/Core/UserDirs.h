//
// Where the user's things live: home, the user config dirs, logs, state (docs/windows-port.md §3, docs/install-layout.md).
// One table per platform, and the one owner of $HOME / %USERPROFILE% - nothing else reads them.
//
// Resolve() is a pure function of (the platform, the environment), so every platform's table is tested on any OS.
// Current() is this process's: the build's platform and the real environment, resolved once.
//
//                  Linux                         macOS                         Windows
//   home           $HOME                         $HOME                         %USERPROFILE%
//   assetDirs      ~/.config/goatedit,           (as Linux)                    %USERPROFILE%\.goatedit
//                  ~/.goatedit (legacy)
//   stateDir       ~/.local/state/goatedit       (as Linux)                    %LOCALAPPDATA%\GoatEdit
//   logDir         stateDir                      ~/Library/Logs/GoatEdit       %LOCALAPPDATA%\GoatEdit\Logs
//   systemDataDirs $XDG_DATA_DIRS                (as Linux)                    - (none: assets sit next to the exe)
//
// '~/.config' is $XDG_CONFIG_HOME and '~/.local/state' $XDG_STATE_HOME when those are absolute (a relative or empty
// value is invalid per the XDG spec and the default is used).
//

#ifndef GOATEDIT_USERDIRS_H
#define GOATEDIT_USERDIRS_H

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace gedit {
    class UserDirs {
    public:
        enum class kPlatform : uint8_t {
            kLinux,
            kMacOS,
            kWindows,
        };
        // A variable's value, nullopt when unset
        using EnvLookup = std::function<std::optional<std::string>(const std::string &name)>;

    public:
        static UserDirs Resolve(kPlatform platform, const EnvLookup &env);
        // This process's, resolved on first use
        static const UserDirs &Current();
        // Resolve again - for a test that changed the environment
        static void Reset();
        static kPlatform CurrentPlatform();

        // Shorthand for Current().home
        static const std::filesystem::path &Home();

    public:
        // Empty when the environment doesn't say
        std::filesystem::path home;
        // User asset dirs (config.yml, themes, questions, tools): the preferred one - written to - first, legacy
        // read-only ones after it. Never '$XDG_DATA_HOME/goatedit': a per-user install puts its system assets there.
        std::vector<std::filesystem::path> assetDirs;
        // User-scope state (the terminal's command history)
        std::filesystem::path stateDir;
        std::filesystem::path logDir;
        // Where a system install may have put its assets, besides the exe's own dir
        std::vector<std::filesystem::path> systemDataDirs;
        // git's global ignore file (core.excludesFile's default)
        std::filesystem::path gitGlobalIgnoreFile;
    };
}

#endif //GOATEDIT_USERDIRS_H

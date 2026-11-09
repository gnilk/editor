//
// Where GoatEdit's files live - see docs/install-layout.md
//

#ifndef EDITOR_INSTALLPATHS_H
#define EDITOR_INSTALLPATHS_H

#include <filesystem>
#include <optional>
#include <vector>

namespace gedit {
    // The lookup rules are pure functions of their inputs (exe dir, XDG dirs) so they can be tested
    // without touching the real install; only ExecutableDir asks the OS.
    class InstallPaths {
    public:
        // The directory holding the running executable (symlinks resolved), empty if it can't be determined
        static std::filesystem::path ExecutableDir();

        // System asset directories in priority order, existence not checked
        static std::vector<std::filesystem::path> SystemAssetCandidates(const std::filesystem::path &exeDir,
                                                                        const std::vector<std::filesystem::path> &xdgDataDirs);
        // The first candidate holding a config.yml - the one and only system asset directory
        static std::optional<std::filesystem::path> FindSystemAssetDir(const std::vector<std::filesystem::path> &candidates);

        // User asset directories, the preferred (written) one first, legacy read-only fallbacks after it.
        // Never '$XDG_DATA_HOME/goatedit': that is where a per-user install puts its system assets.
        static std::vector<std::filesystem::path> UserAssetDirs(const std::filesystem::path &home,
                                                                const std::filesystem::path &xdgConfigHome);
        // Logs and user-scope history
        static std::filesystem::path UserStateDir(const std::filesystem::path &xdgStateHome);
    };
}

#endif //EDITOR_INSTALLPATHS_H

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
        // The running executable itself (symlinks resolved), empty if it can't be determined - what a tool runs to get
        // the same editor ('goatedit --dump')
        static std::filesystem::path ExecutablePath();

        // System asset directories in priority order, existence not checked
        static std::vector<std::filesystem::path> SystemAssetCandidates(const std::filesystem::path &exeDir,
                                                                        const std::vector<std::filesystem::path> &xdgDataDirs);
        // The first candidate holding a config.yml - the one and only system asset directory
        static std::optional<std::filesystem::path> FindSystemAssetDir(const std::vector<std::filesystem::path> &candidates);
        // The user's dirs are UserDirs'
    };
}

#endif //EDITOR_INSTALLPATHS_H

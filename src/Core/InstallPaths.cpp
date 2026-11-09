//
// Where GoatEdit's files live - see docs/install-layout.md
//

#if defined(GEDIT_MACOS)
#include <mach-o/dyld.h>
#include <climits>
#endif

#include "InstallPaths.h"

using namespace gedit;

// Lower case on every platform, not the app name
static const char *glbDirName = "goatedit";

std::filesystem::path InstallPaths::ExecutableDir() {
    std::error_code ec;
#if defined(GEDIT_LINUX)
    auto exePath = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
        return {};
    }
#elif defined(GEDIT_MACOS)
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) != 0) {
        return {};
    }
    // May hold symlinks or '..' - canonical resolves both
    auto exePath = std::filesystem::canonical(buffer, ec);
    if (ec) {
        return {};
    }
#else
    std::filesystem::path exePath;
#endif
    return exePath.parent_path();
}

std::vector<std::filesystem::path> InstallPaths::SystemAssetCandidates(const std::filesystem::path &exeDir,
                                                                       const std::vector<std::filesystem::path> &xdgDataDirs) {
    std::vector<std::filesystem::path> candidates;
    if (!exeDir.empty()) {
        // A build tree, the macOS bundle, then any install prefix (bin/ next to share/)
        candidates.push_back(exeDir / "resources");
        candidates.push_back((exeDir / ".." / "SharedSupport").lexically_normal());
        candidates.push_back((exeDir / ".." / "share" / glbDirName).lexically_normal());
    }
    for (auto &dataDir : xdgDataDirs) {
        candidates.push_back(dataDir / glbDirName);
    }
    return candidates;
}

std::optional<std::filesystem::path> InstallPaths::FindSystemAssetDir(const std::vector<std::filesystem::path> &candidates) {
    for (auto &candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate / "config.yml", ec)) {
            return candidate;
        }
    }
    return {};
}

std::vector<std::filesystem::path> InstallPaths::UserAssetDirs(const std::filesystem::path &home,
                                                               const std::filesystem::path &xdgConfigHome) {
    return {
        xdgConfigHome / glbDirName,     // preferred
        home / ".goatedit",             // legacy
    };
}

std::filesystem::path InstallPaths::UserStateDir(const std::filesystem::path &xdgStateHome) {
    return xdgStateHome / glbDirName;
}

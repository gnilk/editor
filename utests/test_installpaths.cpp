//
// Install layout and asset lookup - docs/install-layout.md
//
#include <testinterface.h>
#include <algorithm>
#include <fstream>
#include "Core/InstallPaths.h"
#include "Core/AssetLoaderBase.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_installpaths(ITesting *t);
DLL_EXPORT int test_installpaths_candidates(ITesting *t);
DLL_EXPORT int test_installpaths_buildtree(ITesting *t);
DLL_EXPORT int test_installpaths_prefix(ITesting *t);
DLL_EXPORT int test_installpaths_xdgfallback(ITesting *t);
DLL_EXPORT int test_installpaths_userorder(ITesting *t);
DLL_EXPORT int test_installpaths_preferredwins(ITesting *t);
DLL_EXPORT int test_installpaths_executable(ITesting *t);
}

namespace {
    // A scratch tree under the temp dir, removed when the fixture goes
    struct ScratchTree {
        std::filesystem::path root;
        explicit ScratchTree(const std::string &name) : root(std::filesystem::temp_directory_path() / name) {
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
            std::filesystem::create_directories(root);
        }
        ~ScratchTree() {
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
        }
        void WriteFile(const std::filesystem::path &relPath, const std::string &content) const {
            std::filesystem::create_directories((root / relPath).parent_path());
            std::ofstream f(root / relPath);
            f << content;
        }
    };
}

DLL_EXPORT int test_installpaths(ITesting *t) {
    return kTR_Pass;
}

// Exe-relative candidates come first (build tree, bundle, prefix), the XDG data dirs last
DLL_EXPORT int test_installpaths_candidates(ITesting *t) {
    auto candidates = InstallPaths::SystemAssetCandidates("/opt/goat/bin", {"/usr/local/share", "/usr/share"});
    TR_ASSERT(t, candidates.size() == 5);
    TR_ASSERT(t, candidates[0] == "/opt/goat/bin/resources");
    TR_ASSERT(t, candidates[1] == "/opt/goat/SharedSupport");
    TR_ASSERT(t, candidates[2] == "/opt/goat/share/goatedit");
    TR_ASSERT(t, candidates[3] == "/usr/local/share/goatedit");
    TR_ASSERT(t, candidates[4] == "/usr/share/goatedit");

    // Unknown exe dir: only the XDG fallback is left
    candidates = InstallPaths::SystemAssetCandidates({}, {"/usr/share"});
    TR_ASSERT(t, candidates.size() == 1);
    return kTR_Pass;
}

// A build tree wins over an installed copy - the dev binary never picks up a stale install
DLL_EXPORT int test_installpaths_buildtree(ITesting *t) {
    ScratchTree tree("goat_ip_buildtree");
    tree.WriteFile("bld/resources/config.yml", "# build\n");
    tree.WriteFile("installed/share/goatedit/config.yml", "# installed\n");

    auto found = InstallPaths::FindSystemAssetDir(
        InstallPaths::SystemAssetCandidates(tree.root / "bld", {tree.root / "installed" / "share"}));
    TR_ASSERT(t, found.has_value());
    TR_ASSERT(t, *found == tree.root / "bld" / "resources");
    return kTR_Pass;
}

// An install prefix (~/.local, /usr/local, an AppImage's usr/): bin/goatedit finds ../share/goatedit
DLL_EXPORT int test_installpaths_prefix(ITesting *t) {
    ScratchTree tree("goat_ip_prefix");
    tree.WriteFile("prefix/share/goatedit/config.yml", "# installed\n");
    std::filesystem::create_directories(tree.root / "prefix" / "bin");
    // A directory without a config.yml is not an asset dir, even when it exists
    std::filesystem::create_directories(tree.root / "prefix" / "bin" / "resources");

    auto found = InstallPaths::FindSystemAssetDir(InstallPaths::SystemAssetCandidates(tree.root / "prefix" / "bin", {}));
    TR_ASSERT(t, found.has_value());
    TR_ASSERT(t, *found == tree.root / "prefix" / "share" / "goatedit");
    return kTR_Pass;
}

DLL_EXPORT int test_installpaths_xdgfallback(ITesting *t) {
    ScratchTree tree("goat_ip_xdg");
    tree.WriteFile("data2/goatedit/config.yml", "# packaged\n");
    std::filesystem::create_directories(tree.root / "elsewhere" / "bin");

    auto found = InstallPaths::FindSystemAssetDir(
        InstallPaths::SystemAssetCandidates(tree.root / "elsewhere" / "bin", {tree.root / "data1", tree.root / "data2"}));
    TR_ASSERT(t, found.has_value());
    TR_ASSERT(t, *found == tree.root / "data2" / "goatedit");

    // Nothing anywhere
    found = InstallPaths::FindSystemAssetDir(InstallPaths::SystemAssetCandidates(tree.root / "elsewhere" / "bin", {}));
    TR_ASSERT(t, !found.has_value());
    return kTR_Pass;
}

// The XDG config dir is preferred (it's where new files are written); the legacy dirs follow
DLL_EXPORT int test_installpaths_userorder(ITesting *t) {
    auto dirs = InstallPaths::UserAssetDirs("/home/goat", "/home/goat/.config");
    TR_ASSERT(t, dirs.size() == 2);
    TR_ASSERT(t, dirs[0] == "/home/goat/.config/goatedit");
    TR_ASSERT(t, dirs[1] == "/home/goat/.goatedit");
    // A per-user install's system assets ('~/.local' prefix) are never read as user config
    auto installed = InstallPaths::SystemAssetCandidates("/home/goat/.local/bin", {})[2];
    TR_ASSERT(t, installed == "/home/goat/.local/share/goatedit");
    TR_ASSERT(t, std::find(dirs.begin(), dirs.end(), installed) == dirs.end());
    TR_ASSERT(t, InstallPaths::UserStateDir("/home/goat/.local/state") == "/home/goat/.local/state/goatedit");
    return kTR_Pass;
}

// Within a location type the loader keeps registration order: a hit in a legacy dir must not promote it
// above the preferred dir for later loads (the old hit-count ordering did exactly that)
DLL_EXPORT int test_installpaths_preferredwins(ITesting *t) {
    ScratchTree tree("goat_ip_order");
    tree.WriteFile("preferred/config.yml", "preferred\n");
    tree.WriteFile("legacy/config.yml", "legacy\n");
    tree.WriteFile("legacy/only_legacy.yml", "legacy\n");

    AssetLoaderBase loader;
    loader.AddSearchPath(tree.root / "preferred", AssetLoaderBase::kLocationType::kUser);
    loader.AddSearchPath(tree.root / "legacy", AssetLoaderBase::kLocationType::kUser);

    for (int i = 0; i < 3; i++) {
        TR_ASSERT(t, loader.LoadTextAsset("only_legacy.yml", AssetLoaderBase::kLocationType::kUser) != nullptr);
    }
    auto asset = loader.LoadTextAsset("config.yml", AssetLoaderBase::kLocationType::kUser);
    TR_ASSERT(t, asset != nullptr);
    TR_ASSERT(t, asset->GetOriginPath() == tree.root / "preferred" / "config.yml");
    TR_ASSERT(t, loader.ResolveWritePath("new.yml", AssetLoaderBase::kLocationType::kUser) == tree.root / "preferred" / "new.yml");
    return kTR_Pass;
}

// The running executable (trun here) is found - the lookup's anchor
DLL_EXPORT int test_installpaths_executable(ITesting *t) {
    auto exeDir = InstallPaths::ExecutableDir();
    TR_ASSERT(t, !exeDir.empty());
    TR_ASSERT(t, exeDir.is_absolute());
    TR_ASSERT(t, std::filesystem::is_directory(exeDir));
    return kTR_Pass;
}

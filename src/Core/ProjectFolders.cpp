//
// ProjectFolders - see ProjectFolders.h
//

#include <algorithm>
#include <fmt/format.h>

#include "ProjectFolders.h"
#include "Core/Config/Config.h"
#include "Core/PathUtil.h"
#include "Core/Platform.h"
#include "Core/UserDirs.h"

using namespace gedit;
namespace fs = std::filesystem;

// The relative path from 'base' to 'path', empty when there is none (another drive, an error)
static fs::path RelativeTo(const fs::path &path, const fs::path &base) {
    if (path.empty() || base.empty()) {
        return {};
    }
    std::error_code ec;
    auto rel = fs::relative(path, base, ec);
    return ec ? fs::path() : rel;
}

// In 'base' or below it ('base' itself included)
static bool IsWithinTree(const fs::path &path, const fs::path &base) {
    auto rel = RelativeTo(path, base);
    if (rel.empty()) {
        return false;
    }
    // A copy: MSVC's path iterator holds the element itself - a reference into the temporary dangles
    const auto first = *rel.begin();
    return first != "..";
}

// Strictly below 'base' - not 'base' itself
static bool IsStrictSubdir(const fs::path &path, const fs::path &base) {
    auto rel = RelativeTo(path, base);
    if (rel.empty()) {
        return false;
    }
    const auto first = *rel.begin();
    return (first != ".") && (first != "..");
}

static bool IsFilesystemRoot(const fs::path &path) {
    auto canonical = pathutil::CanonicalPath(path);
    return canonical == canonical.root_path();
}

ProjectFolders::ProjectFolders(fs::path pathHome, std::vector<fs::path> projectFolders, std::vector<fs::path> systemDirs) :
    home(std::move(pathHome)), systemFolders(std::move(systemDirs)) {
    // A drive root or home itself as a project folder would widen the rule to everything on it - dropped
    for (auto &folder : projectFolders) {
        if (!folder.empty() && folder.is_absolute() && !IsTooWide(folder)) {
            folders.push_back(std::move(folder));
        }
    }
}

ProjectFolders ProjectFolders::Current() {
    std::vector<fs::path> folders;
    std::vector<fs::path> systemFolders;
    if (UserDirs::CurrentPlatform() == UserDirs::kPlatform::kWindows) {
        auto &config = Config::Instance();
        if (config.HasKey("windows") && config["windows"].HasKey("project_folders")) {
            for (auto &folder : config["windows"].GetSequenceOfStr("project_folders")) {
                folders.push_back(pathutil::ExpandUser(folder, UserDirs::Home()));
            }
        }
        for (auto name : {"WINDIR", "ProgramFiles", "ProgramFiles(x86)", "ProgramData"}) {
            auto value = Platform::GetEnv(name);
            if (value.has_value() && !value->empty()) {
                systemFolders.emplace_back(*value);
            }
        }
    }
    return ProjectFolders(UserDirs::Home(), std::move(folders), std::move(systemFolders));
}

bool ProjectFolders::CanHoldProject(const fs::path &root) const {
    if (IsInSystemFolder(root) || IsTooWide(root)) {
        return false;
    }
    if (IsStrictSubdir(root, home)) {
        return true;
    }
    return std::any_of(folders.begin(), folders.end(), [&root](const fs::path &folder) {
        return IsStrictSubdir(root, folder);
    });
}

bool ProjectFolders::IsWithin(const fs::path &dir) const {
    if (IsWithinTree(dir, home)) {
        return true;
    }
    return std::any_of(folders.begin(), folders.end(), [&dir](const fs::path &folder) {
        return IsWithinTree(dir, folder);
    });
}

bool ProjectFolders::IsTooWide(const fs::path &root) const {
    if (!home.empty() && IsWithinTree(home, root)) {
        return true;
    }
    if (IsFilesystemRoot(root)) {
        return true;
    }
    return std::any_of(systemFolders.begin(), systemFolders.end(), [&root](const fs::path &system) {
        return IsWithinTree(system, root);
    });
}

bool ProjectFolders::IsInSystemFolder(const fs::path &path) const {
    return std::any_of(systemFolders.begin(), systemFolders.end(), [&path](const fs::path &system) {
        return IsWithinTree(path, system);
    });
}

std::string ProjectFolders::NotAProjectHint(const fs::path &folder, const fs::path &configFile) const {
    if (!CouldHoldProject(folder)) {
        return {};
    }
    return fmt::format("'{}' is outside {}, so it isn't a project - to make it one, {}", folder.string(), OutsideWhat(),
                       AddFolderAdvice(folder, configFile));
}

std::string ProjectFolders::StartedOutsideHint(const fs::path &startDir, const fs::path &configFile) const {
    if (!CouldHoldProject(startDir)) {
        return {};
    }
    return fmt::format("started in '{}', outside {} - working in '{}'. If it is a project folder, {}", startDir.string(),
                       OutsideWhat(), home.string(), AddFolderAdvice(startDir, configFile));
}

bool ProjectFolders::CouldHoldProject(const fs::path &folder) const {
    return !folder.empty() && !IsWithin(folder) && !IsTooWide(folder) && !IsInSystemFolder(folder);
}

std::string ProjectFolders::AddFolderAdvice(const fs::path &folder, const fs::path &configFile) const {
    auto parent = pathutil::CanonicalPath(folder).parent_path();
    // The parent too when it could be a project folder itself - not 'C:\' or a system folder
    bool isParentUseful = !parent.empty() && !IsTooWide(parent) && !IsInSystemFolder(parent);
    auto what = isParentUseful ? fmt::format("add it (or '{}')", parent.string()) : std::string("add it");
    return fmt::format("{} to windows.project_folders in {}", what, configFile.string());
}

std::string ProjectFolders::OutsideWhat() const {
    return folders.empty() ? std::string("your home folder") : std::string("your home folder and project folders");
}

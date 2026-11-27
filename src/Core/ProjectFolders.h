//
// Where a project may live (docs/windows-port.md §3.2). A project root is a real sub-folder of home - on every
// platform - or, on Windows, of a folder the user config names, since code there often sits in 'C:\src':
//
//      windows:
//        project_folders: [ 'C:\src', 'D:\src' ]
//
// Never a project: home itself, a folder that holds home, a drive / filesystem root, a system folder (%WINDIR%,
// %ProgramFiles%) or anything inside one. A launch outside all of them moves to home.
//
// A pure value: the folders are given, so every rule is tested on any OS. Current() is this process's - home from
// UserDirs, the folders from the config (Windows only - unix keeps its rule as it is), the system folders from the
// environment. The key is the user's: a project config never widens it (ProjectConfigPolicy).
//

#ifndef GOATEDIT_PROJECTFOLDERS_H
#define GOATEDIT_PROJECTFOLDERS_H

#include <filesystem>
#include <string>
#include <vector>

namespace gedit {
    class ProjectFolders {
    public:
        explicit ProjectFolders(std::filesystem::path pathHome, std::vector<std::filesystem::path> projectFolders = {},
                                std::vector<std::filesystem::path> systemDirs = {});
        // Home, 'windows.project_folders' from the config loaded so far, %WINDIR% / %ProgramFiles% - on each call
        static ProjectFolders Current();

        // A real sub-folder of home or of a project folder, and in no system folder - a project ('.goatedit') may be here
        bool CanHoldProject(const std::filesystem::path &root) const;
        // In home or a project folder (the folder itself too) - a launch from here stays here
        bool IsWithin(const std::filesystem::path &dir) const;
        // Home, a folder that holds it, a filesystem root, a system folder or one that holds it - never indexed as a
        // project, wherever it is
        bool IsTooWide(const std::filesystem::path &root) const;

        // Why 'folder' can't be a project and how to make it one - for a folder outside home and every project folder
        // that the setting could make one; empty when it can't help (already allowed, a root, a system folder, home's
        // own parents). 'startDir' is where the editor started, moved from to home; 'configFile' the user config.
        std::string NotAProjectHint(const std::filesystem::path &folder, const std::filesystem::path &configFile) const;
        std::string StartedOutsideHint(const std::filesystem::path &startDir, const std::filesystem::path &configFile) const;

        const std::filesystem::path &Home() const { return home; }
        const std::vector<std::filesystem::path> &Folders() const { return folders; }

    private:
        bool IsInSystemFolder(const std::filesystem::path &path) const;
        // A folder the setting could make a project: outside every allowed place, but no root / system folder
        bool CouldHoldProject(const std::filesystem::path &folder) const;
        // "add it (or 'parent') to windows.project_folders in <file>"
        std::string AddFolderAdvice(const std::filesystem::path &folder, const std::filesystem::path &configFile) const;
        std::string OutsideWhat() const;

    private:
        std::filesystem::path home;
        std::vector<std::filesystem::path> folders;
        std::vector<std::filesystem::path> systemFolders;
    };
}

#endif //GOATEDIT_PROJECTFOLDERS_H

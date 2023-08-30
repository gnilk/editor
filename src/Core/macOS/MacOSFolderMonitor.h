//
// Created by gnilk on 29.08.23.
//

#ifndef GOATEDIT_MACOSFOLDERMONITOR_H
#define GOATEDIT_MACOSFOLDERMONITOR_H

#include <string>
#include <filesystem>
#include <mutex>

#include <CoreServices/CoreServices.h>

#include "Core/FolderMonitor.h"

namespace gedit {

    class MacOSFolderMonitor : public FolderMonitor {
    public:
        virtual ~MacOSFolderMonitor() = default;
        static MacOSFolderMonitor &Instance();

        bool Start() override;
        bool Stop() override;
        void OnFSEvent(const std::string &path, kChangeFlags flags);
    protected:
        bool AddPath(const std::filesystem::path &path, const std::vector<std::string> &exclusions) override;
    private:
        MacOSFolderMonitor() = default;
        // Need one event stream per watched path - since exclusion lists are specific per stream
        struct MonitorItem {
            std::filesystem::path pathName = {};
            std::vector<std::string> exclusions = {};

            CFStringRef pathToWatch = {};
            CFArrayRef pathsToWatch = {};

            CFMutableArrayRef pathsToExclude = {};

            FSEventStreamRef eventStream = {};
        };
        std::vector<MonitorItem> folderList;
        std::mutex dataGuard;

    };
}


#endif //GOATEDIT_MACOSFOLDERMONITOR_H

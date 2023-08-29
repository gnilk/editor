//
// Created by gnilk on 29.08.23.
//

#ifndef GOATEDIT_MACOSFOLDERMONITOR_H
#define GOATEDIT_MACOSFOLDERMONITOR_H

#include <string>
#include <filesystem>
#include <mutex>

#include <CoreServices/CoreServices.h>

#include "Core/FolderMonitorBase.h"

namespace gedit {

    class MacOSFolderMonitor : public FolderMonitorBase {
    public:
        virtual ~MacOSFolderMonitor() = default;
        static MacOSFolderMonitor &Instance();

        bool Start() override;
        bool Stop() override;
        void OnFSEvent(const std::string &path, kChangeFlags flags);
    protected:
        bool AddPath(const std::filesystem::path &path) override;
    private:
        MacOSFolderMonitor() = default;
        bool isRunning = false;
        std::vector<std::string> folderList;
        std::mutex dataGuard;
        FSEventStreamRef eventStream;

    };
}


#endif //GOATEDIT_MACOSFOLDERMONITOR_H

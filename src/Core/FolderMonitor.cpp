//
// Created by gnilk on 29.08.23.
//
#include "FolderMonitor.h"
#include "StrUtil.h"
#include "Glob.h"
#include "RuntimeConfig.h"
#include "PathUtil.h"
#include "Core/Config/Config.h"

static std::string cfgSectionName = "foldermonitor";

using namespace gedit;

FolderMonitor::MonitorPoint::Ref FolderMonitor::CreateMonitorPoint(const std::filesystem::path &pathToMonitor, EventDelegate handler) {
    auto monitor = MonitorPoint::Create(pathToMonitor);
    monitor->SetEventHandler(handler);
    return monitor;
}

// protoected
void FolderMonitor::MonitorPoint::DispatchEvent(const std::string &fullPathName, kChangeFlags eventFlags) {
// Note: We can support more/less stuff with exclusion lists here....
//    if (Glob::Match(listener.pattern, name) == Glob::kMatch::Match) {

    // Put this on the main thread...
    RuntimeConfig::Instance().GetRootView().PostMessage([this, fullPathName, eventFlags](){
        handler(fullPathName, eventFlags);
    });
}

// Support functions
bool FolderMonitor::IsEnabled() {
    if (!Config::Instance()[cfgSectionName].GetBool("enabled", true)) {
        return false;
    }
    return true;
}

const std::vector<std::string> &FolderMonitor::GetExclusionPaths() {
    exclusionPaths = Config::Instance()[cfgSectionName].GetSequenceOfStr("exclude");
    if (!Config::Instance()[cfgSectionName].GetBool("use_git_ignore", true)) {
        // TODO: Parse .gitignore here
    }

    return exclusionPaths;
}


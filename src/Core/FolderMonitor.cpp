//
// Created by gnilk on 29.08.23.
//
#include "FolderMonitor.h"
#include "StrUtil.h"
#include "Glob.h"
#include "RuntimeConfig.h"
#include "PathUtil.h"

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


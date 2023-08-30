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
    return nullptr;
}



bool FolderMonitor::AddEventListener(const std::string &globPattern, const std::vector<std::string> &excludePaths, EventDelegate handler) {

    std::string pathNoGlob = globPattern;
    auto pos = pathNoGlob.find_first_of("*");
    if (pos != std::string::npos) {
        pathNoGlob = globPattern.substr(0,pos);
    }

    // Track if we were running - so we can automatically restart...
    bool bWasRunning = IsRunning();
    if (!AddPath(pathNoGlob, excludePaths)) {
        // ouppsie..
        return false;
    }

    Listener listener {
        .pattern = globPattern,
        .handler = handler,
    };
    eventListeners.push_back(listener);


    if (bWasRunning) {
        return Start();
    }
    return true;
}

// protoected
void FolderMonitor::DispatchEvent(const std::string &name, kChangeFlags eventFlags) {
    for(auto &listener : eventListeners) {
        if (Glob::Match(listener.pattern, name) == Glob::kMatch::Match) {
            // Put this on the main thread...
            RuntimeConfig::Instance().GetRootView().PostMessage([listener, name, eventFlags](){
                listener.handler(name, eventFlags);
            });
        }
    }
}

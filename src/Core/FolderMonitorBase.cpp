//
// Created by gnilk on 29.08.23.
//
#include "FolderMonitorBase.h"
#include "StrUtil.h"
#include "Glob.h"
#include "RuntimeConfig.h"
#include "PathUtil.h"

using namespace gedit;

bool FolderMonitorBase::AddEventListener(const std::string &globPattern, EventDelegate handler) {

    std::string pathNoGlob = globPattern;
    auto pos = pathNoGlob.find_first_of("*");
    if (pos != std::string::npos) {
        pathNoGlob = globPattern.substr(0,pos);
    }

    // FIXME: remove up 'til first pattern...
    if (!AddPath(pathNoGlob)) {
        // ouppsie..
        return false;
    }

    Listener listener {
        .pattern = globPattern,
        .handler = handler,
    };
    eventListeners.push_back(listener);
    return true;
}

// protoected
void FolderMonitorBase::DispatchEvent(const std::string &name, kChangeFlags eventFlags) {
    for(auto &listener : eventListeners) {
        if (Glob::Match(listener.pattern, name) == Glob::kMatch::Match) {
            // Put this on the main thread...
            RuntimeConfig::Instance().GetRootView().PostMessage([listener, name, eventFlags](){
                listener.handler(name, eventFlags);
            });
        }
    }
}

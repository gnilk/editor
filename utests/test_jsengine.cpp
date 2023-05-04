//
// Created by gnilk on 02.05.23.
//
#include <testinterface.h>
#include "Core/JSEngine/JSPluginEngine.h"
#include "Core/RuntimeConfig.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_jsengine(ITesting *t);
    DLL_EXPORT int test_jsengine_init(ITesting *t);
    DLL_EXPORT int test_jsengine_builtin(ITesting *t);
    DLL_EXPORT int test_jsengine_garbagecollection(ITesting *t);
}

DLL_EXPORT int test_jsengine(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_jsengine_init(ITesting *t) {
    JSPluginEngine jsEngine;
    if (!jsEngine.Initialize()) {
        return kTR_Fail;
    }
    return kTR_Pass;
}

DLL_EXPORT int test_jsengine_builtin(ITesting *t) {
//    JSPluginEngine jsEngine;
//    if (!jsEngine.Initialize()) {
//        return kTR_Fail;
//    }
    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("setlanguage"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("setlanguage");
    TR_ASSERT(t, cmd != nullptr);
    cmd->Execute({".cpp"});
    return kTR_Pass;
}
DLL_EXPORT int test_jsengine_garbagecollection(ITesting *t) {
//    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("setlanguage"));
//    auto cmd = RuntimeConfig::Instance().GetPluginCommand("setlanguage");
//    TR_ASSERT(t, cmd != nullptr);
//    cmd->Execute({".cpp"});
    return kTR_Pass;
}

//
// Created by gnilk on 07.08.23.
//
#include <testinterface.h>
#include "Core/XDGEnvironment.h"
#include "Core/StrUtil.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_xgd(ITesting *t);
DLL_EXPORT int test_xgd_resolve(ITesting *t);
DLL_EXPORT int test_xgd_syslists(ITesting *t);
DLL_EXPORT int test_xgd_sysfirst(ITesting *t);
DLL_EXPORT int test_xgd_firstprefix(ITesting *t);
}

DLL_EXPORT int test_xgd(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_xgd_resolve(ITesting *t) {
    std::string home(getenv("HOME"));
    auto instance = XDGEnvironment::Instance();
    auto userData = instance.GetUserDataPath();
    TR_ASSERT(t, strutil::startsWith(userData, home));
    return kTR_Pass;
}

DLL_EXPORT int test_xgd_syslists(ITesting *t) {
    auto instance = XDGEnvironment::Instance();
    auto sysDataPaths = instance.GetSystemDataPaths();
    TR_ASSERT(t, sysDataPaths.size() != 0);
    return kTR_Pass;

}

DLL_EXPORT int test_xgd_sysfirst(ITesting *t) {
    auto instance = XDGEnvironment::Instance();
    auto sysDataPath = instance.GetFirstSystemDataPath();
    TR_ASSERT(t, !sysDataPath.empty());
    return kTR_Pass;
}

DLL_EXPORT int test_xgd_firstprefix(ITesting *t) {
    auto instance = XDGEnvironment::Instance();
    auto sysDataPath = instance.GetFirstSystemDataPathWithPrefix("/usr/local");
    TR_ASSERT(t, sysDataPath.has_value());
    TR_ASSERT(t, !sysDataPath.value().empty());
    TR_ASSERT(t, strutil::startsWith(sysDataPath.value(), "/usr/local"));
    return kTR_Pass;

}

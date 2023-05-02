//
// Created by gnilk on 02.05.23.
//
#include <testinterface.h>
#include <filesystem>
#include "Core/JSEngine/JSWrapper.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_jswrapper(ITesting *t);
    DLL_EXPORT int test_jswrapper_init(ITesting *t);
}

DLL_EXPORT int test_jswrapper(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_jswrapper_init(ITesting *t) {
    JSWrapper jsWrapper;
    if (!jsWrapper.Initialize()) {
        return kTR_Fail;
    }
    return kTR_Pass;
}

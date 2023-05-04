//
// Created by gnilk on 02.05.23.
//
#include <testinterface.h>
#include "logger.h"
#include "Core/Editor.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_main(ITesting *t);
}

DLL_EXPORT int test_main(ITesting *t) {
    // We need this..
    Editor::Instance().Initialize(0, nullptr);
    return kTR_Pass;
}

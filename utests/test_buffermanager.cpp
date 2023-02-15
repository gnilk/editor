//
// Created by gnilk on 15.02.23.
//
#include <testinterface.h>
#include "Core/BufferManager.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_main(ITesting *t);
    DLL_EXPORT int test_buffermgr(ITesting *t);
    DLL_EXPORT int test_buffermgr_newbuffer(ITesting *t);
}
DLL_EXPORT int test_main(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_buffermgr(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_buffermgr_newbuffer(ITesting *t) {
    auto buffer = BufferManager::Instance().NewBuffer("buf1");
    TR_ASSERT(t, buffer != nullptr);
    auto buf2 = BufferManager::Instance().NewBuffer("buf1");
    TR_ASSERT(t, buf2 == nullptr);
    return kTR_Pass;
}


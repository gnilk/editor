//
// Created by gnilk on 19.08.23.
//
#include <testinterface.h>
#include "Core/Config/ConfigNode.h"


using namespace gedit;

extern "C" {
DLL_EXPORT int test_config(ITesting *t);
DLL_EXPORT int test_config_setint(ITesting *t);
}

DLL_EXPORT int test_config(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_config_setint(ITesting *t) {
    ConfigNode node;

    node.SetInt("test1", 25);
    TR_ASSERT(t, node.GetInt("test1", -1) == 25);
    node.SetInt("test2", 50);
    TR_ASSERT(t, node.GetInt("test2", -1) == 50);

    auto s = node.ToString();
    printf("str: %s\n", s.c_str());
    return kTR_Pass;
}

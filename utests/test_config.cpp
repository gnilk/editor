//
// Created by gnilk on 19.08.23.
//
#include <testinterface.h>
#include "Core/Config/ConfigNode.h"


using namespace gedit;

extern "C" {
DLL_EXPORT int test_config(ITesting *t);
DLL_EXPORT int test_config_setint(ITesting *t);
DLL_EXPORT int test_config_setdyn(ITesting *t);
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

enum kDummyTest : int {
    kApa = 1,
    kBpa = 2,
};
DLL_EXPORT int test_config_setdyn(ITesting *t) {
    ConfigNode node;

    node.SetValue<int>("bla",2);
    TR_ASSERT(t, node.GetInt("bla", -1) == 2);

    node.SetValue<std::string>("bla2","hello");
    TR_ASSERT(t, node.GetStr("bla2", "wef") == "hello");

    node.SetValue<int>("enum", kDummyTest::kApa);
    TR_ASSERT(t, node.GetValue<int>("enum", -1) == kDummyTest::kApa);

    return kTR_Pass;
}
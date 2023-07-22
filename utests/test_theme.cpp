//
// Created by gnilk on 22.07.23.
//
#include <testinterface.h>
#include "Core/Theme/Theme.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_theme(ITesting *t);
DLL_EXPORT int test_theme_create(ITesting *t);
DLL_EXPORT int test_theme_load(ITesting *t);
}

DLL_EXPORT int test_theme(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_theme_create(ITesting *t) {
    auto theme = Theme::Create();
    TR_ASSERT(t, theme != nullptr);
    return kTR_Pass;
}
// colors.json
DLL_EXPORT int test_theme_load(ITesting *t) {
    auto theme = Theme::Create();
    auto ok = theme->Load("default.theme.yml");
    TR_ASSERT(t, ok);
    return kTR_Pass;
}

//
// Created by gnilk on 22.07.23.
//
#include <testinterface.h>
#include <cmath>
#include "Core/Theme/Theme.h"
#include "Core/Sublime/SublimeConfigColorScript.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_theme(ITesting *t);
DLL_EXPORT int test_theme_create(ITesting *t);
DLL_EXPORT int test_theme_load(ITesting *t);
DLL_EXPORT int test_theme_alpha(ITesting *t);
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

// Sublime's alpha() is a 0..1 fraction; the SublimeConfigColorScript import boundary must yield a
// ColorRGBA whose alpha is always in [0,1] — never a 0..255 magnitude (the old `alpha(224)` defect
// that turned the selection green in Gansi and faint-by-accident in SDL). Pin that here so it can't
// regress.
DLL_EXPORT int test_theme_alpha(ITesting *t) {
    SublimeConfigColorScript engine;
    engine.RegisterBuiltIn();

    // A normal 0..1 alpha is stored verbatim.
    auto [ok1, c1] = engine.ExecuteColorScript("alpha(0.12)");
    TR_ASSERT(t, ok1);
    TR_ASSERT(t, fabs(c1.A() - 0.12f) < 0.001f);

    auto [ok2, c2] = engine.ExecuteColorScript("alpha(0.5)");
    TR_ASSERT(t, ok2);
    TR_ASSERT(t, fabs(c2.A() - 0.5f) < 0.001f);

    // An out-of-range value (a 0..255 magnitude authored by mistake) is clamped to opaque, NOT left
    // as a 224.0 that would overflow downstream.
    auto [ok3, c3] = engine.ExecuteColorScript("alpha(224)");
    TR_ASSERT(t, ok3);
    TR_ASSERT(t, fabs(c3.A() - 1.0f) < 0.001f);

    // The full theme path: color(<base> alpha(x)) sets the base's alpha to x, staying in [0,1].
    auto [ok4, c4] = engine.ExecuteColorScript("color(rgb(255,165,0), alpha(0.12))");
    TR_ASSERT(t, ok4);
    TR_ASSERT(t, fabs(c4.A() - 0.12f) < 0.001f);
    TR_ASSERT(t, c4.A() >= 0.0f && c4.A() <= 1.0f);

    return kTR_Pass;
}

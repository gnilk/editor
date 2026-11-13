//
// Created by gnilk on 22.07.23.
//
#include <testinterface.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include "Core/Theme/Theme.h"
#include "Core/Sublime/SublimeConfigColorScript.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_theme(ITesting *t);
DLL_EXPORT int test_theme_create(ITesting *t);
DLL_EXPORT int test_theme_load(ITesting *t);
DLL_EXPORT int test_theme_alpha(ITesting *t);
DLL_EXPORT int test_theme_sharedbackground(ITesting *t);
DLL_EXPORT int test_theme_loadmissing(ITesting *t);
DLL_EXPORT int test_theme_loadbroken(ITesting *t);
DLL_EXPORT int test_theme_loadordefault(ITesting *t);
DLL_EXPORT int test_theme_reloadkeeps(ITesting *t);
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

// One background for the whole editor: a section that sets no background/foreground of its own takes the global one,
// so the terminal and the text area match the rest of the UI unless a theme says otherwise.
DLL_EXPORT int test_theme_sharedbackground(ITesting *t) {
    auto theme = Theme::Create();
    TR_ASSERT(t, theme->Load("default.theme.yml"));
    auto globalBg = theme->GetGlobalColors().GetColor("background");
    auto globalFg = theme->GetGlobalColors().GetColor("foreground");
    TR_ASSERT(t, theme->GetGlobalColors().HasColor("background"));
    TR_ASSERT(t, theme->GetContentColors().GetColor("background") == globalBg);
    TR_ASSERT(t, theme->GetTerminalColor().GetColor("background") == globalBg);
    TR_ASSERT(t, theme->GetTerminalColor().GetColor("foreground") == globalFg);
    return kTR_Pass;
}

// open-bugs #29: a theme that can't be loaded crashed the editor at the first parse - the colour accessors dereferenced
// a null table. A failed load says why, and the accessors give empty tables.
DLL_EXPORT int test_theme_loadmissing(ITesting *t) {
    auto theme = Theme::Create();
    TR_ASSERT(t, !theme->Load("no-such-theme.theme.yml"));
    TR_ASSERT(t, !theme->GetLoadError().empty());
    TR_ASSERT(t, !theme->GetGlobalColors().HasColor("background"));
    TR_ASSERT(t, !theme->GetContentColors().HasColor("background"));
    TR_ASSERT(t, !theme->GetUIColors().HasColor("background"));
    TR_ASSERT(t, !theme->GetTerminalColor().HasColor("background"));
    return kTR_Pass;
}

// Theme files in a folder of their own; the colour file named by its full path (the asset loader takes one as it is)
static std::filesystem::path WriteTheme(const std::string &colorsText) {
    auto dir = std::filesystem::temp_directory_path() / "goatedit_test_theme";
    std::filesystem::create_directories(dir);
    auto colorsPath = dir / "test.colors.json";
    if (!colorsText.empty()) {
        std::ofstream(colorsPath) << colorsText;
    } else {
        std::filesystem::remove(colorsPath);
    }
    auto themePath = dir / "test.theme.yml";
    std::ofstream(themePath) << "name: test\ncolorfile: \"" << colorsPath.string() << "\"\n";
    return themePath;
}

static const char *goodColors = R"json({
  "colors": {
    "globals": { "background": "rgb(1,2,3)", "foreground": "rgb(200,200,200)" },
    "content": { "background": "rgb(1,2,3)", "regular": "rgb(200,200,200)" }
  }
})json";

DLL_EXPORT int test_theme_loadbroken(ITesting *t) {
    // The colour file isn't JSON: reported, not thrown
    auto themePath = WriteTheme("{ \"colors\": { ");
    auto theme = Theme::Create();
    TR_ASSERT(t, !theme->Load(themePath.string()));
    TR_ASSERT(t, theme->GetLoadError().find("parsed") != std::string::npos);

    // No colour file
    themePath = WriteTheme("");
    theme = Theme::Create();
    TR_ASSERT(t, !theme->Load(themePath.string()));
    TR_ASSERT(t, theme->GetLoadError().find("missing") != std::string::npos);

    // A colour file without the editor's colours
    themePath = WriteTheme("{ \"colors\": {} }");
    theme = Theme::Create();
    TR_ASSERT(t, !theme->Load(themePath.string()));

    themePath = WriteTheme(goodColors);
    theme = Theme::Create();
    TR_ASSERT(t, theme->Load(themePath.string()));
    TR_ASSERT(t, theme->GetGlobalColors().HasColor("background"));
    return kTR_Pass;
}

DLL_EXPORT int test_theme_loadordefault(ITesting *t) {
    std::string problem;
    auto theme = Theme::LoadOrDefault("dcscode-2026-dark.theme.yml", problem);
    TR_ASSERT(t, theme != nullptr);
    TR_ASSERT(t, problem.find("dcscode-2026-dark.theme.yml") != std::string::npos);
    TR_ASSERT(t, problem.find(Theme::DEFAULT_THEME) != std::string::npos);

    auto shipped = Theme::Create();
    TR_ASSERT(t, shipped->Load(Theme::DEFAULT_THEME));
    TR_ASSERT(t, theme->GetGlobalColors().GetColor("background") == shipped->GetGlobalColors().GetColor("background"));

    // A theme that loads: no problem
    theme = Theme::LoadOrDefault(Theme::DEFAULT_THEME, problem);
    TR_ASSERT(t, theme != nullptr);
    TR_ASSERT(t, problem.empty());
    return kTR_Pass;
}

// A file broken since the last load: the reload fails and the colours stay
DLL_EXPORT int test_theme_reloadkeeps(ITesting *t) {
    auto themePath = WriteTheme(goodColors);
    auto theme = Theme::Create();
    TR_ASSERT(t, theme->Load(themePath.string()));
    auto background = theme->GetGlobalColors().GetColor("background");

    WriteTheme("not json");
    TR_ASSERT(t, !theme->Reload());
    TR_ASSERT(t, theme->GetGlobalColors().HasColor("background"));
    TR_ASSERT(t, theme->GetGlobalColors().GetColor("background") == background);
    std::filesystem::remove_all(themePath.parent_path());
    return kTR_Pass;
}

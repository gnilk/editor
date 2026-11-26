//
// Created by gnilk on 02.05.23.
//
#include <testinterface.h>
#include <filesystem>
#include "logger.h"
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/UnicodeHelper.h"
#include "Core/Platform.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_main(ITesting *t);
}

class UTestConsole : public IOutputConsole {
public:
    void WriteLine(const std::u32string &str) override;
};
static UTestConsole utestConsole;

void UTestConsole::WriteLine(const std::u32string &str) {
    auto str8 = UnicodeHelper::utf32toascii(str);
    fprintf(stdout, "%s\n", str8.c_str());
    fflush(stdout);
}

// The build tree's assets: next to this library, not next to the executable (that's trun) and never an
// installed copy - docs/install-layout.md
static std::string TestAssetDir() {
    auto libPath = Platform::ModulePathOf(reinterpret_cast<const void *>(&test_main));
    if (libPath.empty()) {
        return "resources";
    }
    return (libPath.parent_path() / "resources").string();
}

DLL_EXPORT int test_main(ITesting *t) {
    static const std::string assetDir = TestAssetDir();
    // '--unit-test': the working directory stays where trun runs (the fixtures are relative to it), also outside $HOME
    static const char *argv[]={"goatedit.app", "--console-logging", "--skip-user-config", "--unit-test", "--backend",
                               "headless", "--asset-dir", assetDir.c_str()};
    Editor::Instance().Initialize(8, argv);
    // Disable auto-save in unit-testing
    Config::Instance()["main"].SetInt("autosave_timeout_ms", 0);

    RuntimeConfig::Instance().SetOutputConsole(&utestConsole);
    return kTR_Pass;
}

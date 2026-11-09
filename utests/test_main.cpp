//
// Created by gnilk on 02.05.23.
//
#include <testinterface.h>
#include <dlfcn.h>
#include <filesystem>
#include "logger.h"
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/UnicodeHelper.h"

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
    Dl_info info = {};
    if ((dladdr(reinterpret_cast<void *>(&test_main), &info) == 0) || (info.dli_fname == nullptr)) {
        return "resources";
    }
    std::error_code ec;
    auto libPath = std::filesystem::canonical(info.dli_fname, ec);
    if (ec) {
        return "resources";
    }
    return (libPath.parent_path() / "resources").string();
}

DLL_EXPORT int test_main(ITesting *t) {
    static const std::string assetDir = TestAssetDir();
    static const char *argv[]={"goatedit.app", "--console-logging", "--skip-user-config", "--backend", "headless",
                               "--asset-dir", assetDir.c_str()};
    Editor::Instance().Initialize(7, argv);
    // Disable auto-save in unit-testing
    Config::Instance()["main"].SetInt("autosave_timeout_ms", 0);

    RuntimeConfig::Instance().SetOutputConsole(&utestConsole);
    return kTR_Pass;
}

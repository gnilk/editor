//
// Created by gnilk on 02.05.23.
//
#include <testinterface.h>
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

DLL_EXPORT int test_main(ITesting *t) {
    // We need this..
    static const char *argv[]={"goatedit.app", "--console_logging", "--skip_user_config"};
    Editor::Instance().Initialize(3, argv);
    // Disable auto-save in unit-testing
    Config::Instance()["main"].SetInt("autosave_timeout_ms", 0);

    RuntimeConfig::Instance().SetOutputConsole(&utestConsole);
    return kTR_Pass;
}

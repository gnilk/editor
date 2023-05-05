//
// Created by gnilk on 02.05.23.
//
#include <testinterface.h>
#include "logger.h"
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_main(ITesting *t);
}

class UTestConsole : public IOutputConsole {
public:
    void WriteLine(const std::string &str) override;
};
static UTestConsole utestConsole;

void UTestConsole::WriteLine(const std::string &str) {
    fprintf(stdout, "%s\n", str.c_str());
    fflush(stdout);
}

DLL_EXPORT int test_main(ITesting *t) {
    // We need this..
    Editor::Instance().Initialize(0, nullptr);
    RuntimeConfig::Instance().SetOutputConsole(&utestConsole);
    return kTR_Pass;
}

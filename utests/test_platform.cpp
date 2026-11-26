//
// Platform (src/Core/Platform.h) - what the operating system does differently, behind plain functions
// (docs/windows-port.md §2). Run on whichever platform built the library.
//
#include <testinterface.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <string>

#include "Core/Platform.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_platform(ITesting *t);
    DLL_EXPORT int test_platform_name(ITesting *t);
    DLL_EXPORT int test_platform_env(ITesting *t);
    DLL_EXPORT int test_platform_localtime(ITesting *t);
    DLL_EXPORT int test_platform_exepath(ITesting *t);
}

DLL_EXPORT int test_platform(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_platform_name(ITesting *t) {
    auto name = Platform::Name();
#if defined(GEDIT_MACOS)
    TR_ASSERT(t, name == "macos");
#elif defined(GEDIT_LINUX)
    TR_ASSERT(t, name == "linux");
#endif
    // The keymap and config.yml sections are lower case
    TR_ASSERT(t, !name.empty());
    for (auto ch : name) {
        TR_ASSERT(t, (ch >= 'a') && (ch <= 'z'));
    }
    return kTR_Pass;
}

DLL_EXPORT int test_platform_env(ITesting *t) {
    static const std::string kName = "GOATEDIT_TEST_PLATFORM_ENV";
    Platform::UnsetEnv(kName);
    TR_ASSERT(t, !Platform::GetEnv(kName).has_value());
    TR_ASSERT(t, Platform::GetEnvOr(kName, "fallback") == "fallback");

    // UTF-8 goes through as it is
    Platform::SetEnv(kName, "räksmörgås");
    TR_ASSERT(t, Platform::GetEnv(kName) == "räksmörgås");
    TR_ASSERT(t, Platform::GetEnvOr(kName, "fallback") == "räksmörgås");

    // Empty is a value for GetEnv, the fallback for GetEnvOr
    Platform::SetEnv(kName, "");
    TR_ASSERT(t, Platform::GetEnv(kName).has_value());
    TR_ASSERT(t, Platform::GetEnv(kName)->empty());
    TR_ASSERT(t, Platform::GetEnvOr(kName, "fallback") == "fallback");

    Platform::UnsetEnv(kName);
    TR_ASSERT(t, !Platform::GetEnv(kName).has_value());
    return kTR_Pass;
}

DLL_EXPORT int test_platform_localtime(ITesting *t) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto local = Platform::LocalTime(now);
    // Back again: mktime reads a local time
    TR_ASSERT(t, std::mktime(&local) == now);
    TR_ASSERT(t, local.tm_year >= 126);
    return kTR_Pass;
}

DLL_EXPORT int test_platform_exepath(ITesting *t) {
    // trun loads the library, so the executable is trun
    auto exePath = Platform::ExecutablePath();
    TR_ASSERT(t, !exePath.empty());
    TR_ASSERT(t, exePath.is_absolute());
    TR_ASSERT(t, std::filesystem::exists(exePath));
    TR_ASSERT(t, exePath.filename().string().find("trun") != std::string::npos);
    return kTR_Pass;
}

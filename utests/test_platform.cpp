//
// Platform (src/Core/Platform.h) - what the operating system does differently, behind plain functions
// (docs/windows-port.md §2). Run on whichever platform built the library.
//
#include <testinterface.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "Core/Platform.h"
#include "Core/PathUtil.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_platform(ITesting *t);
    DLL_EXPORT int test_platform_name(ITesting *t);
    DLL_EXPORT int test_platform_env(ITesting *t);
    DLL_EXPORT int test_platform_localtime(ITesting *t);
    DLL_EXPORT int test_platform_exepath(ITesting *t);
    DLL_EXPORT int test_platform_atomicwrite(ITesting *t);
    DLL_EXPORT int test_platform_expanduser(ITesting *t);
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

static std::string ReadAll(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

DLL_EXPORT int test_platform_atomicwrite(ITesting *t) {
    auto dir = std::filesystem::temp_directory_path() / "goatedit_platform_atomicwrite";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    auto file = dir / "session.yml";

    TR_ASSERT(t, Platform::WriteFileAtomic(file, "first\n"));
    TR_ASSERT(t, ReadAll(file) == "first\n");
    // Over an existing file: replaced whole, the temp file gone
    TR_ASSERT(t, Platform::WriteFileAtomic(file, "second"));
    TR_ASSERT(t, ReadAll(file) == "second");
    TR_ASSERT(t, !std::filesystem::exists(dir / "session.yml.tmp"));
    // A dir that isn't there: fails, writes nothing
    TR_ASSERT(t, !Platform::WriteFileAtomic(dir / "missing" / "x.yml", "x"));
    TR_ASSERT(t, !std::filesystem::exists(dir / "missing"));

    std::filesystem::remove_all(dir);
    return kTR_Pass;
}

DLL_EXPORT int test_platform_expanduser(ITesting *t) {
    std::filesystem::path home = "/home/goat";
    TR_ASSERT(t, pathutil::ExpandUser("~", home) == home);
    TR_ASSERT(t, pathutil::ExpandUser("~/", home) == home);
    TR_ASSERT(t, pathutil::ExpandUser("~/.gedit", home) == home / ".gedit");
    TR_ASSERT(t, pathutil::ExpandUser("~/a/b.js", home) == home / "a/b.js");
    // Windows' separator after the '~'
    TR_ASSERT(t, pathutil::ExpandUser("~\\.gedit", home) == home / ".gedit");
    // Left as they are
    TR_ASSERT(t, pathutil::ExpandUser("~other/x", home) == "~other/x");
    TR_ASSERT(t, pathutil::ExpandUser("bootstrap.script", home) == "bootstrap.script");
    TR_ASSERT(t, pathutil::ExpandUser("/abs/~/x", home) == "/abs/~/x");
    TR_ASSERT(t, pathutil::ExpandUser("", home).empty());
    // No home: nothing to expand to
    TR_ASSERT(t, pathutil::ExpandUser("~/.gedit", {}) == "~/.gedit");
    return kTR_Pass;
}

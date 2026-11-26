//
// Platform (src/Core/Platform.h) for Linux and macOS. Where the two differ the '#if' stays here (docs/windows-port.md §2).
//

#include <cstdlib>
#include <pthread.h>

#if defined(GEDIT_MACOS)
#include <mach-o/dyld.h>
#include <climits>
#endif

#include "Core/Platform.h"

using namespace gedit;

std::string_view Platform::Name() {
#if defined(GEDIT_MACOS)
    return "macos";
#else
    return "linux";
#endif
}

void Platform::SetThreadName(const char *name) {
#if defined(GEDIT_MACOS)
    // macOS names only the calling thread
    pthread_setname_np(name);
#else
    pthread_setname_np(pthread_self(), name);
#endif
}

std::tm Platform::LocalTime(std::time_t time) {
    std::tm local = {};
    localtime_r(&time, &local);
    return local;
}

std::optional<std::string> Platform::GetEnv(const std::string &name) {
    const char *value = ::getenv(name.c_str());
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
}

std::string Platform::GetEnvOr(const std::string &name, const std::string &fallback) {
    auto value = GetEnv(name);
    if (!value.has_value() || value->empty()) {
        return fallback;
    }
    return *value;
}

void Platform::SetEnv(const std::string &name, const std::string &value) {
    ::setenv(name.c_str(), value.c_str(), 1);
}

void Platform::UnsetEnv(const std::string &name) {
    ::unsetenv(name.c_str());
}

std::filesystem::path Platform::ExecutablePath() {
    std::error_code ec;
#if defined(GEDIT_MACOS)
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) != 0) {
        return {};
    }
    // May hold symlinks or '..' - canonical resolves both
    auto exePath = std::filesystem::canonical(buffer, ec);
#else
    auto exePath = std::filesystem::read_symlink("/proc/self/exe", ec);
#endif
    if (ec) {
        return {};
    }
    return exePath;
}

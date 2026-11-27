//
// Platform (src/Core/Platform.h) for Linux and macOS. Where the two differ the '#if' stays here (docs/windows-port.md §2).
//

#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

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

std::filesystem::path Platform::ModulePathOf(const void *address) {
    Dl_info info = {};
    if ((dladdr(address, &info) == 0) || (info.dli_fname == nullptr)) {
        return {};
    }
    std::error_code ec;
    auto modulePath = std::filesystem::canonical(info.dli_fname, ec);
    return ec ? std::filesystem::path() : modulePath;
}

std::optional<std::filesystem::path> Platform::FindExecutable(const std::string &name, const std::string &pathVariable,
                                                              const std::vector<std::filesystem::path> &moreDirs) {
    if (name.empty() || (name.find('/') != std::string::npos)) {
        return {};
    }
    std::vector<std::filesystem::path> dirs;
    size_t start = 0;
    while (start <= pathVariable.size()) {
        auto end = pathVariable.find(':', start);
        if (end == std::string::npos) {
            end = pathVariable.size();
        }
        if (end > start) {
            dirs.emplace_back(pathVariable.substr(start, end - start));
        }
        start = end + 1;
    }
    dirs.insert(dirs.end(), moreDirs.begin(), moreDirs.end());
    for (auto &dir : dirs) {
        auto candidate = dir / name;
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec) && (::access(candidate.c_str(), X_OK) == 0)) {
            return candidate;
        }
    }
    return {};
}

// Stage to <file>.tmp, fsync, then rename over the target (rename is atomic on a POSIX filesystem)
bool Platform::WriteFileAtomic(const std::filesystem::path &path, const std::string &content) {
    std::filesystem::path tmpPath = path;
    tmpPath += ".tmp";

    int fd = ::open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return false;
    }
    size_t remaining = content.size();
    const char *ptr = content.data();
    while (remaining > 0) {
        ssize_t n = ::write(fd, ptr, remaining);
        if (n < 0) {
            ::close(fd);
            ::unlink(tmpPath.c_str());
            return false;
        }
        ptr += n;
        remaining -= static_cast<size_t>(n);
    }
    ::fsync(fd);
    ::close(fd);

    std::error_code ec;
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }
    return true;
}

// A unix program has no console of its own to give back - the terminal it runs in is the user's
void Platform::ReleaseOwnConsole() {
}

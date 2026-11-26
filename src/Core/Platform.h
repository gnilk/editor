//
// What the operating system does differently, behind plain functions (docs/windows-port.md §2). This header names no
// platform: each platform's source file defines these - 'unix/PlatformUnix.cpp' for Linux and macOS - and the build
// picks it, as it does for Process and SystemFonts. Code elsewhere calls these instead of an '#ifdef'.
//
// Strings are UTF-8 on every platform (Windows converts at its edge).
//

#ifndef GOATEDIT_PLATFORM_H
#define GOATEDIT_PLATFORM_H

#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gedit::Platform {
    // 'linux', 'macos' or 'windows' - the keymap's and config.yml's platform section
    std::string_view Name();

    // Names the calling thread for debuggers and crash reports. Linux cuts the name at 15 characters
    void SetThreadName(const char *name);

    // The local time of 'time' (localtime_r / localtime_s)
    std::tm LocalTime(std::time_t time);

    // The variable's value, nullopt when it isn't set (an empty value is a value)
    std::optional<std::string> GetEnv(const std::string &name);
    // The value, or 'fallback' when it is unset or empty
    std::string GetEnvOr(const std::string &name, const std::string &fallback);
    void SetEnv(const std::string &name, const std::string &value);
    void UnsetEnv(const std::string &name);

    // The running executable, symlinks resolved; empty when the system won't say
    std::filesystem::path ExecutablePath();

    // An executable called 'name' (a plain name, no directory part): in the dirs of 'pathVariable' (a PATH value, in
    // the platform's syntax), then in 'moreDirs'. Windows also tries the PATHEXT extensions ('claude' finds claude.cmd)
    std::optional<std::filesystem::path> FindExecutable(const std::string &name, const std::string &pathVariable,
                                                        const std::vector<std::filesystem::path> &moreDirs = {});

    // Writes 'content' so that a crash leaves either the old file or the new one, never half of it: a temp file next
    // to it, flushed to disk, then put in its place
    bool WriteFileAtomic(const std::filesystem::path &path, const std::string &content);
}

#endif //GOATEDIT_PLATFORM_H

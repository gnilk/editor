//
// Platform (src/Core/Platform.h) for Windows (docs/windows-port.md §2.1). UTF-8 in and out; the W functions inside.
//

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <thread>
#include <vector>

#include "WinString.h"
#include "Core/Platform.h"

using namespace gedit;
using namespace gedit::winstring;

std::string_view Platform::Name() {
    return "windows";
}

void Platform::SetThreadName(const char *name) {
    // Windows 10 1607+; shown by debuggers and in crash dumps
    SetThreadDescription(GetCurrentThread(), Widen(name).c_str());
}

std::tm Platform::LocalTime(std::time_t time) {
    std::tm local = {};
    localtime_s(&local, &time);
    return local;
}

std::optional<std::string> Platform::GetEnv(const std::string &name) {
    auto wideName = Widen(name);
    DWORD length = GetEnvironmentVariableW(wideName.c_str(), nullptr, 0);
    if (length == 0) {
        // Unset - or set and empty, which Windows can't tell apart from the size alone
        return (GetLastError() == ERROR_ENVVAR_NOT_FOUND) ? std::nullopt : std::optional<std::string>(std::string());
    }
    std::wstring value((size_t)length, L'\0');
    length = GetEnvironmentVariableW(wideName.c_str(), value.data(), length);
    value.resize(length);
    return Narrow(value);
}

std::string Platform::GetEnvOr(const std::string &name, const std::string &fallback) {
    auto value = GetEnv(name);
    if (!value.has_value() || value->empty()) {
        return fallback;
    }
    return *value;
}

// Both the CRT's copy (getenv) and the process environment (GetEnvironmentVariable, what a child gets, what GetEnv
// reads). The CRT's first: it can't hold an empty value - '_wputenv_s(name, L"")' removes the variable, from the process
// environment too - so the process environment is set after it, empty values included
void Platform::SetEnv(const std::string &name, const std::string &value) {
    auto wideName = Widen(name);
    auto wideValue = Widen(value);
    _wputenv_s(wideName.c_str(), wideValue.c_str());
    SetEnvironmentVariableW(wideName.c_str(), wideValue.c_str());
}

void Platform::UnsetEnv(const std::string &name) {
    auto wideName = Widen(name);
    SetEnvironmentVariableW(wideName.c_str(), nullptr);
    _wputenv_s(wideName.c_str(), L"");
}

std::filesystem::path Platform::ExecutablePath() {
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true) {
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), (DWORD)buffer.size());
        if (length == 0) {
            return {};
        }
        if (length < buffer.size()) {
            std::error_code ec;
            auto exePath = std::filesystem::canonical(std::filesystem::path(std::wstring(buffer.data(), length)), ec);
            return ec ? std::filesystem::path() : exePath;
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path Platform::ModulePathOf(const void *address) {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)address, &module)) {
        return {};
    }
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true) {
        DWORD length = GetModuleFileNameW(module, buffer.data(), (DWORD)buffer.size());
        if (length == 0) {
            return {};
        }
        if (length < buffer.size()) {
            std::error_code ec;
            auto modulePath = std::filesystem::canonical(std::filesystem::path(std::wstring(buffer.data(), length)), ec);
            return ec ? std::filesystem::path() : modulePath;
        }
        buffer.resize(buffer.size() * 2);
    }
}

// ';' between the dirs; 'name' as it is, then with each PATHEXT extension ('claude' finds claude.exe / claude.cmd)
std::optional<std::filesystem::path> Platform::FindExecutable(const std::string &name, const std::string &pathVariable,
                                                              const std::vector<std::filesystem::path> &moreDirs) {
    if (name.empty() || (name.find_first_of("/\\:") != std::string::npos)) {
        return {};
    }
    std::vector<std::filesystem::path> dirs;
    size_t start = 0;
    while (start <= pathVariable.size()) {
        auto end = pathVariable.find(';', start);
        if (end == std::string::npos) {
            end = pathVariable.size();
        }
        if (end > start) {
            auto dir = pathVariable.substr(start, end - start);
            // PATH entries may be quoted
            if ((dir.size() >= 2) && (dir.front() == '"') && (dir.back() == '"')) {
                dir = dir.substr(1, dir.size() - 2);
            }
            dirs.emplace_back(Widen(dir));
        }
        start = end + 1;
    }
    dirs.insert(dirs.end(), moreDirs.begin(), moreDirs.end());

    std::vector<std::string> names;
    bool hasExtension = std::filesystem::path(name).has_extension();
    if (hasExtension) {
        names.push_back(name);
    }
    auto pathExt = GetEnvOr("PATHEXT", ".COM;.EXE;.BAT;.CMD");
    start = 0;
    while (start <= pathExt.size()) {
        auto end = pathExt.find(';', start);
        if (end == std::string::npos) {
            end = pathExt.size();
        }
        if (end > start) {
            names.push_back(name + pathExt.substr(start, end - start));
        }
        start = end + 1;
    }
    for (auto &dir : dirs) {
        for (auto &candidateName : names) {
            auto candidate = dir / Widen(candidateName);
            std::error_code ec;
            if (std::filesystem::is_regular_file(candidate, ec)) {
                return candidate;
            }
        }
    }
    return {};
}

// A temp file next to it, flushed, then moved over the target. Antivirus and the search indexer hold a just-written
// file open for a moment, so the move is retried briefly.
bool Platform::WriteFileAtomic(const std::filesystem::path &path, const std::string &content) {
    std::filesystem::path tmpPath = path;
    tmpPath += ".tmp";

    HANDLE file = CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    size_t written = 0;
    bool isOk = true;
    while (isOk && (written < content.size())) {
        DWORD chunk = 0;
        auto toWrite = (DWORD)std::min<size_t>(content.size() - written, 1u << 30);
        isOk = WriteFile(file, content.data() + written, toWrite, &chunk, nullptr) && (chunk > 0);
        written += chunk;
    }
    isOk = isOk && FlushFileBuffers(file);
    CloseHandle(file);
    if (!isOk) {
        DeleteFileW(tmpPath.c_str());
        return false;
    }
    for (int attempt = 0; attempt < 10; attempt++) {
        if (MoveFileExW(tmpPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    DeleteFileW(tmpPath.c_str());
    return false;
}

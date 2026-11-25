//
// ConPTY probe (hitlist #81, docs/windows-port.md §4.5): what of a terminal's byte stream survives Windows' pseudo
// console? The editor needs the prompt marks (OSC 133) and the cwd (OSC 7) - it can't ask the kernel for them there -
// and a build's output with its lines whole. Nothing of the editor is needed to find out.
//
//   conpty_probe                    every test; writes conpty_out/report.txt and the raw bytes of each test
//   conpty_probe --only <name>      one of: marks, modes, burst, longline, resize, exit, ctrlc, bash, pwsh, powershell, cmd
//   conpty_probe --conpty <dll>     a conpty.dll (Microsoft's NuGet package, with its OpenConsole.exe next to it)
//                                   instead of the one built into Windows
//   conpty_probe --selftest         the analysis only, on made-up bytes - runs on any OS
//
// Build: see README.md next to this file.
//

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------------------------------------------------
// Analysis: plain functions over the bytes the pseudo console gave us - no Windows here
// ---------------------------------------------------------------------------------------------------------------------

namespace analysis {

    struct Osc {
        size_t at = 0;
        std::string body;   // '133;A'
    };

    // Every OSC in the stream, ended by BEL or ST. One cut off at the end is left out.
    std::vector<Osc> FindOscs(const std::string &bytes) {
        std::vector<Osc> oscs;
        size_t i = 0;
        while ((i = bytes.find("\x1b]", i)) != std::string::npos) {
            size_t start = i + 2;
            size_t end = start;
            while ((end < bytes.size()) && (bytes[end] != '\x07') && (bytes[end] != '\x1b')) {
                end++;
            }
            if (end >= bytes.size()) {
                break;
            }
            oscs.push_back({i, bytes.substr(start, end - start)});
            i = end;
        }
        return oscs;
    }

    // Where the first OSC starting with 'prefix' is, at or after 'from'; npos when there is none
    size_t FindOsc(const std::string &bytes, const std::string &prefix, size_t from = 0) {
        for (auto &osc : FindOscs(bytes)) {
            if ((osc.at >= from) && (osc.body.rfind(prefix, 0) == 0)) {
                return osc.at;
            }
        }
        return std::string::npos;
    }

    size_t CountOsc(const std::string &bytes, const std::string &prefix) {
        size_t count = 0;
        for (auto &osc : FindOscs(bytes)) {
            count += (osc.body.rfind(prefix, 0) == 0) ? 1 : 0;
        }
        return count;
    }

    // The text with the escape sequences taken out - what a reader that ignores cursor movement sees
    std::string StripEscapes(const std::string &bytes) {
        std::string text;
        size_t i = 0;
        while (i < bytes.size()) {
            if (bytes[i] != '\x1b') {
                text += bytes[i++];
                continue;
            }
            if (i + 1 >= bytes.size()) {
                break;
            }
            char kind = bytes[i + 1];
            i += 2;
            if (kind == '[') {
                while ((i < bytes.size()) && !((bytes[i] >= 0x40) && (bytes[i] <= 0x7e))) {
                    i++;
                }
                i++;
            } else if ((kind == ']') || (kind == 'P') || (kind == '_') || (kind == '^')) {
                while ((i < bytes.size()) && (bytes[i] != '\x07') && (bytes[i] != '\x1b')) {
                    i++;
                }
                if ((i < bytes.size()) && (bytes[i] == '\x07')) {
                    i++;
                } else {
                    i += 2;     // ST
                }
            } else if ((kind == '(') || (kind == ')') || (kind == '#')) {
                i++;
            }
        }
        return text;
    }

    // How many of 'L00001' .. 'L<count>' are in the text
    size_t CountNumbered(const std::string &text, size_t count) {
        std::set<std::string> seen;
        for (size_t i = text.find('L'); i != std::string::npos; i = text.find('L', i + 1)) {
            auto token = text.substr(i, 6);
            if ((token.size() == 6) && std::all_of(token.begin() + 1, token.end(), [](char c) { return (c >= '0') && (c <= '9'); })) {
                auto number = std::strtoul(token.c_str() + 1, nullptr, 10);
                if ((number >= 1) && (number <= count)) {
                    seen.insert(token);
                }
            }
        }
        return seen.size();
    }

    std::string Printable(const std::string &bytes, size_t maxLength = 400) {
        std::string out;
        for (size_t i = 0; (i < bytes.size()) && (out.size() < maxLength); i++) {
            auto c = (unsigned char)bytes[i];
            if (c == 0x1b) {
                out += "\\e";
            } else if (c == '\r') {
                out += "\\r";
            } else if (c == '\n') {
                out += "\\n";
            } else if ((c < 0x20) || (c == 0x7f)) {
                char buffer[8];
                snprintf(buffer, sizeof(buffer), "\\x%02x", c);
                out += buffer;
            } else {
                out += (char)c;
            }
        }
        return out;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Report: [yes] / [NO] lines, on the screen and in the file
// ---------------------------------------------------------------------------------------------------------------------

class Report {
public:
    bool Open(const fs::path &path) {
        file = fopen(path.string().c_str(), "wb");
        return file != nullptr;
    }
    ~Report() {
        if (file != nullptr) {
            fclose(file);
        }
    }
    void Line(const std::string &text) {
        printf("%s\n", text.c_str());
        fflush(stdout);
        if (file != nullptr) {
            fprintf(file, "%s\n", text.c_str());
            fflush(file);
        }
    }
    void Head(const std::string &name) {
        Line("");
        Line("== " + name);
    }
    bool Check(bool isOk, const std::string &what) {
        Line(std::string(isOk ? "   [yes]  " : "   [NO]   ") + what);
        return isOk;
    }
    void Info(const std::string &what) {
        Line("   [info] " + what);
    }
    void Skip(const std::string &why) {
        Line("   [skip] " + why);
    }

private:
    FILE *file = nullptr;
};

// What the marks test looks for - shared by the Windows test and the self test. 'bytes' is what came out.
static void JudgeMarks(Report &report, const std::string &bytes) {
    using namespace analysis;
    auto npos = std::string::npos;
    auto a = FindOsc(bytes, "133;A");
    auto b = FindOsc(bytes, "133;B");
    auto c = FindOsc(bytes, "133;C");
    auto d = FindOsc(bytes, "133;D;3");
    report.Check(a != npos, "OSC 133;A (prompt) comes through");
    report.Check(b != npos, "OSC 133;B (command) comes through");
    report.Check(c != npos, "OSC 133;C (output starts) comes through");
    report.Check(d != npos, "OSC 133;D;3 (done, with the exit code) comes through");
    auto prompt = bytes.find("PROMPT>");
    auto command = bytes.find("THECOMMAND");
    auto first = bytes.find("OUT-0001");
    auto last = bytes.find("OUT-0005");
    auto next = bytes.find("NEXTPROMPT>");
    bool hasText = (prompt != npos) && (command != npos) && (first != npos) && (last != npos) && (next != npos);
    report.Check(hasText, "the text comes through");
    bool isOrdered = hasText && (a != npos) && (b != npos) && (c != npos) && (d != npos) &&
                     (a < prompt) && (prompt < b) && (b < command) && (command < c) && (c < first) && (last < d) && (d < next);
    report.Check(isOrdered, "the marks are in their place in the text (A prompt B command C output D prompt)");
    report.Check(FindOsc(bytes, "133;D;4") != npos, "a mark ended by ST (ESC \\) instead of BEL comes through");
    report.Check(FindOsc(bytes, "7;file:") != npos, "OSC 7 (the cwd) comes through");
    if (!isOrdered) {
        report.Info("bytes: " + Printable(bytes, 1200));
    }
}

static std::string MarksScript() {
    return "\x1b]133;A\x07" "PROMPT> " "\x1b]133;B\x07" "THECOMMAND\r\n" "\x1b]133;C\x07"
           "OUT-0001\r\nOUT-0002\r\nOUT-0003\r\nOUT-0004\r\nOUT-0005\r\n" "\x1b]133;D;3\x07"
           "\x1b]7;file://host/C:/src/some%20where\x07"
           "\x1b]133;D;4\x1b\\" "NEXTPROMPT> ";
}

static int SelfTest() {
    using namespace analysis;
    int failed = 0;
    auto expect = [&failed](bool isOk, const char *what) {
        printf("%s %s\n", isOk ? "ok  " : "FAIL", what);
        failed += isOk ? 0 : 1;
    };
    auto script = MarksScript();
    expect(CountOsc(script, "133;") == 5, "five marks in the script");
    expect(FindOsc(script, "133;C") < script.find("OUT-0001"), "C is before the output");
    expect(FindOsc(script, "133;D;3", script.find("NEXTPROMPT>")) == std::string::npos, "'from' is respected");
    expect(FindOscs("abc\x1b]133;A").empty(), "a mark cut off at the end is not one");
    expect(StripEscapes("\x1b[1;31mred\x1b[0m\x1b]0;t\x07x\x1b]7;u\x1b\\y\x1b(Bz") == "redxyz", "escapes taken out");
    expect(CountNumbered("L00001 L00002\x1b[K L00002 LXXXXX L00009", 5) == 2, "numbered lines counted once, in range");
    expect(Printable("\x1b]7;a\x07\r\n") == "\\e]7;a\\x07\\r\\n", "printable");
    printf("-- the judge, on the bytes as sent (every line must be yes):\n");
    Report report;
    JudgeMarks(report, script);
    printf("-- on bytes without any OSC (the marks must all be NO):\n");
    std::string without = "PROMPT> THECOMMAND\r\nOUT-0001\r\nOUT-0005\r\nNEXTPROMPT> ";
    JudgeMarks(report, without);
    printf("%s\n", (failed == 0) ? "self test passed" : "SELF TEST FAILED");
    return (failed == 0) ? 0 : 1;
}

#ifndef _WIN32

int main(int argc, char **argv) {
    if ((argc > 1) && (std::string(argv[1]) == "--selftest")) {
        return SelfTest();
    }
    printf("The probe runs on Windows; here only: %s --selftest\n", argv[0]);
    return 1;
}

#else

// ---------------------------------------------------------------------------------------------------------------------
// Windows
// ---------------------------------------------------------------------------------------------------------------------

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#ifdef _MSC_VER
#pragma comment(lib, "advapi32.lib")
#endif

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

using Clock = std::chrono::steady_clock;

static std::wstring Widen(const std::string &text) {
    int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
    std::wstring wide((size_t)length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), wide.data(), length);
    return wide;
}

static std::string Narrow(const std::wstring &text) {
    int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string narrow((size_t)length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), narrow.data(), length, nullptr, nullptr);
    return narrow;
}

// The pseudo console's functions: Windows' own (kernel32), or a conpty.dll's
struct ConPtyApi {
    using CreateFn = HRESULT (WINAPI *)(COORD, HANDLE, HANDLE, DWORD, void **);
    using ResizeFn = HRESULT (WINAPI *)(void *, COORD);
    using CloseFn = void (WINAPI *)(void *);
    CreateFn create = nullptr;
    ResizeFn resize = nullptr;
    CloseFn close = nullptr;
    std::string source;

    static ConPtyApi &Instance() {
        static ConPtyApi api;
        return api;
    }
    bool Load(const std::string &dll) {
        auto find = [](HMODULE module, const char *first, const char *second) -> FARPROC {
            auto fn = GetProcAddress(module, first);
            return (fn != nullptr) ? fn : GetProcAddress(module, second);
        };
        auto module = dll.empty() ? GetModuleHandleW(L"kernel32.dll") : LoadLibraryW(Widen(dll).c_str());
        if (module == nullptr) {
            return false;
        }
        source = dll.empty() ? "kernel32 (built into Windows)" : dll;
        create = reinterpret_cast<CreateFn>(reinterpret_cast<void *>(find(module, "ConptyCreatePseudoConsole", "CreatePseudoConsole")));
        resize = reinterpret_cast<ResizeFn>(reinterpret_cast<void *>(find(module, "ConptyResizePseudoConsole", "ResizePseudoConsole")));
        close = reinterpret_cast<CloseFn>(reinterpret_cast<void *>(find(module, "ConptyClosePseudoConsole", "ClosePseudoConsole")));
        return (create != nullptr) && (resize != nullptr) && (close != nullptr);
    }
};

// A program in a pseudo console; a thread reads what comes out
class Session {
public:
    Session() = default;
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    ~Session() {
        Close();
    }

    bool Start(const std::string &commandLine, int cols, int rows, const std::vector<std::pair<std::string, std::string>> &env = {}) {
        HANDLE inputRead = nullptr;
        HANDLE outputWrite = nullptr;
        if (!CreatePipe(&inputRead, &inputWrite, nullptr, 0) || !CreatePipe(&outputRead, &outputWrite, nullptr, 0)) {
            error = "CreatePipe failed";
            return false;
        }
        COORD size = {(SHORT)cols, (SHORT)rows};
        auto hr = ConPtyApi::Instance().create(size, inputRead, outputWrite, 0, &console);
        CloseHandle(inputRead);
        CloseHandle(outputWrite);
        if (FAILED(hr)) {
            console = nullptr;
            error = "CreatePseudoConsole failed";
            return false;
        }
        reader = std::thread([this]() { ReadLoop(); });

        SIZE_T listSize = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &listSize);
        std::vector<uint8_t> list(listSize);
        STARTUPINFOEXW info = {};
        info.StartupInfo.cb = sizeof(info);
        // Null handles, on purpose: without this a probe whose own output is redirected (ssh, '> file') hands its
        // handles to the child, and nothing goes through the pseudo console
        info.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        info.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(list.data());
        if (!InitializeProcThreadAttributeList(info.lpAttributeList, 1, 0, &listSize) ||
            !UpdateProcThreadAttribute(info.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, console, sizeof(console), nullptr, nullptr)) {
            error = "the process attribute list failed";
            return false;
        }
        // The child inherits our environment, changed for as long as CreateProcess takes
        for (auto &[name, value] : env) {
            SetEnvironmentVariableW(Widen(name).c_str(), Widen(value).c_str());
        }
        auto command = Widen(commandLine);
        bool isOk = CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE,
                                   EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &info.StartupInfo, &process) != 0;
        auto code = GetLastError();
        for (auto &[name, value] : env) {
            SetEnvironmentVariableW(Widen(name).c_str(), nullptr);
        }
        DeleteProcThreadAttributeList(info.lpAttributeList);
        if (!isOk) {
            error = "CreateProcess failed (" + std::to_string(code) + "): " + commandLine;
        }
        return isOk;
    }

    void Write(const std::string &text) {
        DWORD written = 0;
        WriteFile(inputWrite, text.data(), (DWORD)text.size(), &written, nullptr);
    }
    void Resize(int cols, int rows) {
        COORD size = {(SHORT)cols, (SHORT)rows};
        ConPtyApi::Instance().resize(console, size);
    }
    std::string Bytes() {
        std::lock_guard<std::mutex> guard(lock);
        return bytes;
    }
    bool WaitForExit(int ms) {
        return (process.hProcess != nullptr) && (WaitForSingleObject(process.hProcess, (DWORD)ms) == WAIT_OBJECT_0);
    }
    bool IsOutputEnded() const {
        return isEnded;
    }
    // Until 'predicate' holds for what has come, or 'ms' have gone
    bool WaitFor(const std::function<bool(const std::string &)> &predicate, int ms) {
        auto end = Clock::now() + std::chrono::milliseconds(ms);
        while (Clock::now() < end) {
            if (predicate(Bytes())) {
                return true;
            }
            Sleep(20);
        }
        return predicate(Bytes());
    }
    bool WaitForText(const std::string &text, int ms) {
        return WaitFor([&text](const std::string &got) { return got.find(text) != std::string::npos; }, ms);
    }
    // Nothing new for 'quietMs' - at most 'maxMs'
    void WaitQuiet(int quietMs, int maxMs) {
        auto end = Clock::now() + std::chrono::milliseconds(maxMs);
        size_t last = 0;
        auto changed = Clock::now();
        while (Clock::now() < end) {
            auto now = Bytes().size();
            if (now != last) {
                last = now;
                changed = Clock::now();
            }
            if ((Clock::now() - changed) > std::chrono::milliseconds(quietMs)) {
                return;
            }
            Sleep(20);
        }
    }
    // How long closing the pseudo console took
    int Close() {
        int ms = 0;
        if (console != nullptr) {
            auto start = Clock::now();
            ConPtyApi::Instance().close(console);
            ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
            console = nullptr;
        }
        if (process.hProcess != nullptr) {
            if (WaitForSingleObject(process.hProcess, 2000) != WAIT_OBJECT_0) {
                TerminateProcess(process.hProcess, 99);
            }
            CloseHandle(process.hProcess);
            CloseHandle(process.hThread);
            process = {};
        }
        if (inputWrite != nullptr) {
            CloseHandle(inputWrite);
            inputWrite = nullptr;
        }
        if (reader.joinable()) {
            // Closing the console ends the pipe; a read that hangs on regardless is cancelled
            for (int i = 0; (i < 40) && !isEnded; i++) {
                Sleep(50);
            }
            if (!isEnded) {
                CancelSynchronousIo(reader.native_handle_for_cancel());
            }
            reader.join();
        }
        if (outputRead != nullptr) {
            CloseHandle(outputRead);
            outputRead = nullptr;
        }
        return ms;
    }

    std::string error;

private:
    // std::thread, with the Win32 handle the reader duplicated of itself (std::thread's native handle is a pthread's
    // in some MinGW builds)
    struct ReaderThread {
        std::thread thread;
        HANDLE handle = nullptr;
        ReaderThread &operator=(std::thread &&other) {
            thread = std::move(other);
            return *this;
        }
        bool joinable() const {
            return thread.joinable();
        }
        HANDLE native_handle_for_cancel() const {
            return handle;
        }
        void join() {
            thread.join();
            if (handle != nullptr) {
                CloseHandle(handle);
                handle = nullptr;
            }
        }
    };

    void ReadLoop() {
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &reader.handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
        std::vector<char> buffer(65536);
        DWORD count = 0;
        while (ReadFile(outputRead, buffer.data(), (DWORD)buffer.size(), &count, nullptr) && (count > 0)) {
            {
                std::lock_guard<std::mutex> guard(lock);
                bytes.append(buffer.data(), count);
            }
            AnswerQueries();
        }
        isEnded = true;
    }

    // What a terminal answers (the editor's does): newer pseudo consoles ask what we are when they start, and wait
    void AnswerQueries() {
        auto got = Bytes();
        for (size_t at = got.find("\x1b[", answeredTo); at != std::string::npos; at = got.find("\x1b[", at + 1)) {
            if ((got.compare(at, 3, "\x1b[c") == 0) || (got.compare(at, 4, "\x1b[0c") == 0)) {
                Write("\x1b[?62;22c");
            } else if (got.compare(at, 4, "\x1b[6n") == 0) {
                Write("\x1b[1;1R");
            } else if (got.size() - at < 4) {
                // Cut off by the read - again with the next one
                answeredTo = at;
                return;
            }
        }
        answeredTo = got.empty() ? 0 : got.size() - ((got.back() == '\x1b') ? 1 : 0);
    }

    void *console = nullptr;
    HANDLE inputWrite = nullptr;
    HANDLE outputRead = nullptr;
    PROCESS_INFORMATION process = {};
    std::mutex lock;
    std::string bytes;
    std::atomic<bool> isEnded = false;
    size_t answeredTo = 0;      // the reader's
    ReaderThread reader;
};

// ---------------------------------------------------------------------------------------------------------------------
// The child: writes a test's bytes to its console
// ---------------------------------------------------------------------------------------------------------------------

static void WriteConsoleBytes(const std::string &text) {
    static HANDLE console = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    static bool isReady = false;
    if (!isReady) {
        isReady = true;
        SetConsoleOutputCP(CP_UTF8);
        DWORD mode = 0;
        GetConsoleMode(console, &mode);
        // Without it the console prints the escape characters instead of acting on them
        SetConsoleMode(console, mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    DWORD written = 0;
    WriteFile(console, text.data(), (DWORD)text.size(), &written, nullptr);
}

static constexpr size_t BURST_LINES = 3000;

static int Emit(const std::string &what) {
    if (what == "marks") {
        WriteConsoleBytes(MarksScript());
        Sleep(400);
    } else if (what == "modes") {
        // Each between two letters, so what is lost shows in the raw bytes
        WriteConsoleBytes("a\x1b]0;PROBE-TITLE\x07" "b\x1b[?2004h" "c\x1b[?1004h" "d\x1b[?1000h\x1b[?1006h" "e\x1b[?2026h" "f\x1b[?2026l"
                          "g\x1b[5 q" "h\x1b[38;2;1;2;3mT\x1b[0m" "i\x1b[4:3mU\x1b[0m" "j\x1b]8;;http://x.y\x07L\x1b]8;;\x07" "k\x1b]52;c;QUJD\x07"
                          "l\x1b[3mI\x1b[0m" "m\x1b[?1l" "MODES-DONE\r\n");
        Sleep(400);
        WriteConsoleBytes("\x1b[?1049hALT-SCREEN");
        Sleep(400);
        WriteConsoleBytes("\x1b[?1049lMAIN-SCREEN\r\n");
        Sleep(400);
    } else if (what == "burst") {
        WriteConsoleBytes("\x1b]133;C\x07");
        std::string all;
        for (size_t i = 1; i <= BURST_LINES; i++) {
            char line[64];
            snprintf(line, sizeof(line), "L%05u file.cpp:%u: error: x\r\n", (unsigned)i, (unsigned)i);
            all += line;
        }
        WriteConsoleBytes(all);
        WriteConsoleBytes("\x1b]133;D;1\x07" "BURST-DONE\r\n");
        Sleep(800);
    } else if (what == "longline") {
        // A compiler's line, wider than the console: one line in the bytes, or cut where the console wrapped it?
        WriteConsoleBytes("START" + std::string(300, 'x') + "END\r\nAFTER\r\n");
        Sleep(400);
    } else if (what == "resize") {
        WriteConsoleBytes("\x1b]133;A\x07" "BEFORE-RESIZE\r\n" "\x1b]133;C\x07" "SECOND-LINE\r\n" "READY\r\n");
        Sleep(2500);
        WriteConsoleBytes("AFTER-RESIZE\r\n");
        Sleep(400);
    } else if (what == "sleep") {
        WriteConsoleBytes("SLEEPING\r\n");
        Sleep(20000);
        WriteConsoleBytes("SLEPT-IT-OUT\r\n");
    } else if (what == "exit") {
        WriteConsoleBytes("LAST-WORDS\r\n");
        return 7;
    }
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// The tests
// ---------------------------------------------------------------------------------------------------------------------

struct Probe {
    Report report;
    fs::path outDir = "conpty_out";
    std::string self;

    void Save(const std::string &name, const std::string &bytes) {
        std::ofstream(outDir / (name + ".raw"), std::ios::binary) << bytes;
    }
    std::string EmitCommand(const std::string &what) const {
        return "\"" + self + "\" --emit " + what;
    }
    // A child that writes 'what', to its end; its bytes
    bool Run(const std::string &what, std::string &outBytes) {
        Session session;
        if (!session.Start(EmitCommand(what), 120, 30)) {
            report.Skip(session.error);
            return false;
        }
        session.WaitForExit(15000);
        session.WaitQuiet(400, 4000);
        outBytes = session.Bytes();
        session.Close();
        Save(what, outBytes);
        return true;
    }

    void TestMarks() {
        report.Head("marks: the prompt marks and the cwd, in their place");
        std::string bytes;
        if (Run("marks", bytes)) {
            JudgeMarks(report, bytes);
        }
    }

    void TestModes() {
        report.Head("modes: what else a program sends");
        std::string bytes;
        if (!Run("modes", bytes)) {
            return;
        }
        auto has = [&bytes](const std::string &part) { return bytes.find(part) != std::string::npos; };
        report.Check(has("PROBE-TITLE"), "the title (OSC 0)");
        report.Check(has("\x1b[?2004h"), "bracketed paste on (?2004) - a paste into an agent needs it");
        report.Check(has("\x1b[?1004h"), "focus events on (?1004)");
        report.Check(has("\x1b[?1000h") || has("\x1b[?1003h") || has("\x1b[?1002h"), "mouse reports on (?1000 / ?1006)");
        report.Check(has("\x1b[?2026h"), "synchronized output (?2026) - Claude Code draws with it");
        report.Check(has("38;2;1;2;3"), "a 24-bit colour, exact");
        report.Check(has("4:3m"), "a curly underline");
        report.Check(has("\x1b]8;"), "a hyperlink (OSC 8)");
        report.Check(has("\x1b]52;"), "the clipboard (OSC 52)");
        report.Check(has("\x1b[?1049h") || has("\x1b[?1047h"), "the alternate screen (?1049)");
        report.Check(has("ALT-SCREEN") && has("MAIN-SCREEN"), "the text of both screens");
        report.Info("asked of us at start-up: " + analysis::Printable(bytes.substr(0, std::min<size_t>(bytes.size(), 160))));
        report.Info(std::string("win32 input mode (?9001) asked for: ") + (has("\x1b[?9001h") ? "yes" : "no"));
    }

    void TestBurst() {
        report.Head("burst: " + std::to_string(BURST_LINES) + " lines at once, as a build prints them");
        std::string bytes;
        if (!Run("burst", bytes)) {
            return;
        }
        auto text = analysis::StripEscapes(bytes);
        auto seen = analysis::CountNumbered(text, BURST_LINES);
        report.Check(seen == BURST_LINES, std::to_string(seen) + " of " + std::to_string(BURST_LINES) + " lines came through");
        auto c = analysis::FindOsc(bytes, "133;C");
        auto d = analysis::FindOsc(bytes, "133;D;1");
        auto lastLine = bytes.rfind("L03000");
        report.Check(c != std::string::npos && c < bytes.find("L00001"), "'output starts' is before the first line");
        report.Check((d != std::string::npos) && (lastLine != std::string::npos) && (lastLine < d), "'done' is after the last line");
        report.Info(std::to_string(bytes.size()) + " bytes");
    }

    void TestLongLine() {
        report.Head("longline: a line wider than the console");
        std::string bytes;
        if (!Run("longline", bytes)) {
            return;
        }
        auto text = analysis::StripEscapes(bytes);
        auto start = text.find("STARTxx");
        auto end = text.find("xEND");
        bool isWhole = (start != std::string::npos) && (end != std::string::npos) &&
                       (text.substr(start, end - start).find_first_of("\r\n") == std::string::npos);
        report.Check(isWhole, "it stays one line (no line break put in where it wraps)");
        auto at = bytes.find("STARTxx");
        report.Info("bytes: " + ((at == std::string::npos) ? std::string("-") : analysis::Printable(bytes.substr(at, 460), 600)));
    }

    void TestResize() {
        report.Head("resize: does the console send everything again?");
        Session session;
        if (!session.Start(EmitCommand("resize"), 120, 30)) {
            report.Skip(session.error);
            return;
        }
        session.WaitForText("READY", 5000);
        session.WaitQuiet(300, 1500);
        auto before = session.Bytes();
        session.Resize(80, 20);
        session.WaitQuiet(500, 1500);
        auto after = session.Bytes();
        session.WaitForExit(6000);
        session.WaitQuiet(300, 1500);
        auto bytes = session.Bytes();
        session.Close();
        Save("resize", bytes);
        auto again = after.substr(before.size());
        report.Check(again.find("BEFORE-RESIZE") == std::string::npos, "the text isn't sent again");
        auto numMarks = analysis::CountOsc(bytes, "133;A");
        if (numMarks == 0) {
            report.Skip("the marks didn't come through at all - see 'marks'");
        } else {
            report.Check(numMarks == 1, "the marks aren't sent again (" + std::to_string(numMarks) + " A)");
        }
        report.Check(bytes.find("AFTER-RESIZE") != std::string::npos, "output goes on after it");
        report.Info("sent at the resize: " + analysis::Printable(again, 300));
    }

    void TestExit() {
        report.Head("exit: how we learn that the program ended (hitlist #82)");
        Session session;
        if (!session.Start(EmitCommand("exit"), 120, 30)) {
            report.Skip(session.error);
            return;
        }
        report.Check(session.WaitForExit(5000), "the process ends");
        session.WaitForText("LAST-WORDS", 2000);
        Sleep(1500);
        report.Check(session.Bytes().find("LAST-WORDS") != std::string::npos, "its last output comes through");
        report.Info(std::string("the output pipe ends by itself when the program does: ") + (session.IsOutputEnded() ? "yes" : "no - the process handle must be waited on"));
        Save("exit", session.Bytes());
        auto ms = session.Close();
        report.Check(ms < 1000, "closing the pseudo console returns (" + std::to_string(ms) + " ms)");
    }

    void TestCtrlC() {
        report.Head("ctrlc: Ctrl+C as a byte interrupts a program");
        Session session;
        if (!session.Start(EmitCommand("sleep"), 120, 30)) {
            report.Skip(session.error);
            return;
        }
        session.WaitForText("SLEEPING", 5000);
        Sleep(300);
        session.Write("\x03");
        report.Check(session.WaitForExit(3000), "0x03 ends it");
        session.Close();
    }

    // A real shell, its prompt set up to send the marks, two commands run in it
    void TestShell(const std::string &name, const std::string &commandLine, const std::vector<std::string> &bootstrap,
                   const std::vector<std::pair<std::string, std::string>> &env, const std::string &failingCommand, bool hasDone) {
        report.Info("starts: " + commandLine);
        Session session;
        if (!session.Start(commandLine, 120, 30, env)) {
            report.Skip(session.error);
            return;
        }
        session.WaitQuiet(1200, 15000);
        for (auto &line : bootstrap) {
            session.Write(line + "\r");
            Sleep(250);
        }
        session.WaitQuiet(800, 6000);
        auto start = session.Bytes().size();
        session.Write("echo HELLO-FROM-SHELL\r");
        session.WaitQuiet(800, 6000);
        session.Write(failingCommand + "\r");
        session.WaitQuiet(800, 6000);
        auto bytes = session.Bytes();
        auto tail = bytes.substr(std::min(start, bytes.size()));
        // The cwd mark may come once, before our commands
        report.Check(tail.find("HELLO-FROM-SHELL") != std::string::npos, "a command runs and its output comes through");
        report.Check(analysis::CountOsc(tail, "133;A") >= 2, "prompt marks (133;A): " + std::to_string(analysis::CountOsc(tail, "133;A")));
        report.Check(analysis::FindOsc(bytes, "7;file:") != std::string::npos, "the cwd (OSC 7)");
        if (hasDone) {
            report.Check(analysis::CountOsc(tail, "133;C") >= 1, "output-starts marks (133;C): " + std::to_string(analysis::CountOsc(tail, "133;C")));
            report.Check(analysis::CountOsc(tail, "133;D;0") >= 1, "done, exit code 0 (133;D;0)");
            report.Check(analysis::CountOsc(tail, "133;D;1") >= 1, "done, exit code 1 (133;D;1)");
        }
        Save(name, bytes);
        session.Write("exit\r");
        report.Check(session.WaitForExit(5000), "'exit' ends it");
        session.Close();
    }

    void TestBash() {
        report.Head("bash: Git Bash, with the editor's own bootstrap");
        auto bash = FindGitBash();
        if (bash.empty()) {
            report.Skip("Git for Windows isn't installed");
            return;
        }
        // As TerminalController's BuildShellIntegrationBootstrap, plus OSC 7
        TestShell("bash", "\"" + bash + "\" --login -i", {
            "__goat_osc133_done=",
            "__goat_osc133_precmd() { printf '\\033]133;D;%d\\007' \"$?\"; __goat_osc133_done=; printf '\\033]7;file://%s%s\\007' \"$HOSTNAME\" \"$(cygpath -m \"$PWD\")\"; }",
            "__goat_osc133_preexec() { [ -n \"$__goat_osc133_done\" ] && return; __goat_osc133_done=1; printf '\\033]133;C\\007'; }",
            "PROMPT_COMMAND=\"__goat_osc133_precmd\"",
            "trap '__goat_osc133_preexec' DEBUG",
            "PS1='\\[\\033]133;A\\007\\]$ \\[\\033]133;B\\007\\]'",
        }, {}, "false", true);
    }

    void TestPowerShell(const std::string &name, const std::string &exe) {
        report.Head(name + ": a prompt function sends the marks");
        auto path = FindOnPath(exe);
        if (path.empty()) {
            report.Skip(exe + " isn't on PATH");
            return;
        }
        // 'output starts' (133;C) needs PSReadLine's Enter key handler - the editor's job later; here Enter sends it
        TestShell(name, "\"" + path + "\" -NoLogo -NoProfile", {
            "$e=[char]27; $b=[char]7",
            "function prompt { $c = if ($?) {0} else {1}; \"$e]133;D;$c$b$e]7;file://$env:COMPUTERNAME/$($PWD.Path -replace '\\\\','/')$b$e]133;A${b}PS> $e]133;B$b\" }",
            "Set-PSReadLineKeyHandler -Key Enter -ScriptBlock { [Microsoft.PowerShell.PSConsoleReadLine]::AcceptLine(); [Console]::Write(\"$([char]27)]133;C$([char]7)\") }",
        }, {}, "Get-Item nosuch-file-here", true);
    }

    void TestCmd() {
        report.Head("cmd: PROMPT sends the marks (no 'done' - cmd has no hook for it)");
        TestShell("cmd", "cmd.exe /D", {}, {
            {"PROMPT", "$e]133;A$e\\$e]7;file://localhost/$P$e\\$P$G$e]133;B$e\\"},
        }, "dir nosuch-file-here", false);
    }

    static std::string FindGitBash() {
        for (auto root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
            wchar_t buffer[MAX_PATH] = {};
            DWORD size = sizeof(buffer);
            if (RegGetValueW(root, L"SOFTWARE\\GitForWindows", L"InstallPath", RRF_RT_REG_SZ, nullptr, buffer, &size) == ERROR_SUCCESS) {
                auto path = fs::path(buffer) / "bin" / "bash.exe";
                if (fs::exists(path)) {
                    return Narrow(path.wstring());
                }
            }
        }
        return {};
    }
    static std::string FindOnPath(const std::string &exe) {
        wchar_t buffer[MAX_PATH] = {};
        return (SearchPathW(nullptr, Widen(exe).c_str(), nullptr, MAX_PATH, buffer, nullptr) > 0) ? Narrow(buffer) : std::string();
    }
};

static std::string WindowsVersion() {
    using VersionFn = LONG (WINAPI *)(OSVERSIONINFOW *);
    auto fn = reinterpret_cast<VersionFn>(reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")));
    OSVERSIONINFOW info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    if ((fn == nullptr) || (fn(&info) != 0)) {
        return "unknown";
    }
    DWORD revision = 0;
    DWORD size = sizeof(revision);
    RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"UBR", RRF_RT_REG_DWORD, nullptr, &revision, &size);
    return std::to_string(info.dwMajorVersion) + "." + std::to_string(info.dwMinorVersion) + "." + std::to_string(info.dwBuildNumber) +
           "." + std::to_string(revision) + ((info.dwBuildNumber >= 22000) ? " (Windows 11)" : "");
}

int main(int argc, char **argv) {
    std::string only;
    std::string dll;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--selftest") {
            return SelfTest();
        }
        if ((arg == "--emit") && (i + 1 < argc)) {
            return Emit(argv[i + 1]);
        }
        if ((arg == "--only") && (i + 1 < argc)) {
            only = argv[++i];
        } else if ((arg == "--conpty") && (i + 1 < argc)) {
            dll = argv[++i];
        } else {
            printf("unknown argument '%s' - see the top of conpty_probe.cpp\n", arg.c_str());
            return 1;
        }
    }

    Probe probe;
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    probe.self = Narrow(self);
    std::error_code ec;
    fs::create_directories(probe.outDir, ec);
    probe.report.Open(probe.outDir / "report.txt");
    if (!ConPtyApi::Instance().Load(dll)) {
        probe.report.Line("No pseudo console " + (dll.empty() ? std::string("in this Windows (needs 10 1809 or newer)") : "in " + dll));
        return 1;
    }
    probe.report.Line("ConPTY probe - Windows " + WindowsVersion() + ", pseudo console: " + ConPtyApi::Instance().source);

    std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"marks", [&]() { probe.TestMarks(); }},
        {"modes", [&]() { probe.TestModes(); }},
        {"burst", [&]() { probe.TestBurst(); }},
        {"longline", [&]() { probe.TestLongLine(); }},
        {"resize", [&]() { probe.TestResize(); }},
        {"exit", [&]() { probe.TestExit(); }},
        {"ctrlc", [&]() { probe.TestCtrlC(); }},
        {"bash", [&]() { probe.TestBash(); }},
        {"pwsh", [&]() { probe.TestPowerShell("pwsh", "pwsh.exe"); }},
        {"powershell", [&]() { probe.TestPowerShell("powershell", "powershell.exe"); }},
        {"cmd", [&]() { probe.TestCmd(); }},
    };
    bool isAny = false;
    for (auto &[name, test] : tests) {
        if (only.empty() || (only == name)) {
            isAny = true;
            test();
        }
    }
    if (!isAny) {
        probe.report.Line("no test '" + only + "'");
        return 1;
    }
    probe.report.Line("");
    probe.report.Line("The report and the raw bytes are in " + probe.outDir.string() + " - please send the whole folder.");
    return 0;
}

#endif

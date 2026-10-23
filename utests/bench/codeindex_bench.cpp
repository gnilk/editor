//
// Code index lexer throughput (docs/code-index.md §11 step 0). A plain executable, not a trun case: trun
// links the ASan runtime, which makes every allocation expensive and skews the numbers badly.
//
// Usage (from the build dir - the editor resolves its assets relative to the working directory):
//   ./gedit-bench [path] [path...]       a file or a directory (C/C++ sources under it); default ../src
//
// Files are read before timing. Per corpus it prints lines/s for the editor path (TextBuffer + Reparse),
// the fast path (LangLineTokenizer::TokenizeText) on one thread and on N threads.
//
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "Core/Editor.h"
#include "Core/Config/Config.h"
#include "Core/TextBuffer.h"
#include "Core/UnicodeHelper.h"
#include "Core/Language/LangLineTokenizer.h"

using namespace gedit;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

static std::string ReadFile(const fs::path &path);
static void Bench(const fs::path &root);

int main(int argc, const char **argv) {
    static const char *editorArgv[] = {"gedit-bench", "--skip-user-config", "--backend", "headless"};
    Editor::Instance().Initialize(4, editorArgv);

    if (argc < 2) {
        Bench("../src");
    }
    for (int i = 1; i < argc; i++) {
        Bench(argv[i]);
    }
    return 0;
}

static bool IsSource(const fs::path &path) {
    auto ext = path.extension().string();
    return (ext == ".c") || (ext == ".cpp") || (ext == ".cc") || (ext == ".h") || (ext == ".hpp");
}

static double MsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

static void Bench(const fs::path &root) {
    if (!fs::exists(root)) {
        printf("[bench] %s: not found\n", root.string().c_str());
        return;
    }
    std::vector<std::string> files;
    size_t totalBytes = 0;
    if (fs::is_regular_file(root)) {
        files.push_back(ReadFile(root));
        totalBytes += files.back().size();
    } else {
        for (auto &entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file() && IsSource(entry.path())) {
                files.push_back(ReadFile(entry.path()));
                totalBytes += files.back().size();
            }
        }
    }
    if (files.empty()) {
        printf("[bench] %s: no C/C++ sources\n", root.string().c_str());
        return;
    }

    // UTF-8 -> UTF-32 (once per file, part of indexing)
    auto start = Clock::now();
    std::vector<std::u32string> texts;
    texts.reserve(files.size());
    for (auto &data : files) {
        texts.push_back(UnicodeHelper::utf8to32(data));
    }
    auto msDecode = MsSince(start);

    // A copy has its own state stack - what an indexer thread uses
    auto language = Editor::Instance().GetLanguageForExtension(".cpp");

    // Fast path, one thread
    auto tokenizer = language->Tokenizer();
    size_t totalLines = 0;
    size_t totalTokens = 0;
    start = Clock::now();
    for (auto &text : texts) {
        totalLines += tokenizer.TokenizeText(text, [&totalTokens](size_t, const LangToken &) {
            totalTokens++;
        });
    }
    auto msFast = MsSince(start);

    // Fast path, N threads (a tokenizer copy each, files split round-robin)
    auto numThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
    start = Clock::now();
    {
        std::vector<std::thread> threads;
        for (unsigned idxThread = 0; idxThread < numThreads; idxThread++) {
            threads.emplace_back([&texts, &language, idxThread, numThreads]() {
                auto threadTokenizer = language->Tokenizer();
                for (size_t i = idxThread; i < texts.size(); i += numThreads) {
                    threadTokenizer.TokenizeText(texts[i], [](size_t, const LangToken &) {});
                }
            });
        }
        for (auto &thread : threads) {
            thread.join();
        }
    }
    auto msThreaded = MsSince(start);

    // Editor path - TextBuffer lines + synchronous Reparse
    auto mainConfig = Config::Instance()["main"];
    mainConfig.SetBool("threaded_syntaxparser", false);
    start = Clock::now();
    for (auto &data : files) {
        auto buffer = TextBuffer::CreateEmptyBuffer();
        std::stringstream lines(data);
        std::string lineText;
        while (std::getline(lines, lineText)) {
            buffer->AddLineUTF8(lineText.c_str());
        }
        buffer->SetLanguage(language);
        buffer->Reparse();
    }
    auto msEditor = MsSince(start);

    auto linesPerSec = [totalLines](double ms) {
        return (ms > 0) ? (double)totalLines / (ms / 1000.0) : 0.0;
    };
    printf("[bench] %s - %zu files, %zu lines, %.1f MB, %zu tokens\n", root.string().c_str(),
           files.size(), totalLines, (double)totalBytes / (1024.0 * 1024.0), totalTokens);
    printf("[bench]   utf8->utf32       : %9.1f ms\n", msDecode);
    printf("[bench]   editor path       : %9.1f ms  %10.0f lines/s\n", msEditor, linesPerSec(msEditor));
    printf("[bench]   fast path, 1 thr  : %9.1f ms  %10.0f lines/s\n", msFast, linesPerSec(msFast));
    printf("[bench]   fast path, %2u thr : %9.1f ms  %10.0f lines/s\n", numThreads, msThreaded, linesPerSec(msThreaded));
}

static std::string ReadFile(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream data;
    data << file.rdbuf();
    return data.str();
}

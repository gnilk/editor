//
// BuildOutputParser: errors / warnings out of a build's output (docs/build-messages.md, hitlist #52). The logs in
// testfiles/build-logs/ are real captures (clang through make -w, a CMake configure) or written from gcc's format
// (gcc-ninja.log - no gcc on the Mac it was written on); the escapes in clang-make.log are clang's own colours.
//
#include <testinterface.h>
#include <fstream>
#include <sstream>
#include <string>

#include <filesystem>

#include "Core/Build/BuildOutputParser.h"
#include "Core/Build/BuildMessages.h"
#include "Core/Editor/Modals/BuildMessagesModal.h"
#include "Core/Editor.h"
#include "Core/KeyMappingCache.h"

using namespace gedit;
using kSeverity = BuildMessage::kSeverity;
namespace fs = std::filesystem;

extern "C" {
    DLL_EXPORT int test_buildmessages(ITesting *t);
    DLL_EXPORT int test_buildmessages_clangmake(ITesting *t);
    DLL_EXPORT int test_buildmessages_gccninja(ITesting *t);
    DLL_EXPORT int test_buildmessages_cmake(ITesting *t);
    DLL_EXPORT int test_buildmessages_streaming(ITesting *t);
    DLL_EXPORT int test_buildmessages_wrapped(ITesting *t);
    DLL_EXPORT int test_buildmessages_trun(ITesting *t);
    DLL_EXPORT int test_buildmessages_python(ITesting *t);
    DLL_EXPORT int test_buildmessages_pythonsyntax(ITesting *t);
    DLL_EXPORT int test_buildmessages_cargo(ITesting *t);
    DLL_EXPORT int test_buildmessages_linker(ITesting *t);
    DLL_EXPORT int test_buildmessages_carriagereturn(ITesting *t);
    DLL_EXPORT int test_buildmessages_noplace(ITesting *t);
    DLL_EXPORT int test_buildmessages_makedirs(ITesting *t);
    DLL_EXPORT int test_buildmessages_reset(ITesting *t);
    DLL_EXPORT int test_buildmessages_commands(ITesting *t);
    DLL_EXPORT int test_buildmessages_run(ITesting *t);
    DLL_EXPORT int test_buildmessages_terminals(ITesting *t);
    DLL_EXPORT int test_buildmessages_jobend(ITesting *t);
    DLL_EXPORT int test_buildmessages_commanddone(ITesting *t);
    DLL_EXPORT int test_buildmessages_quiet(ITesting *t);
    DLL_EXPORT int test_buildmessages_resolve(ITesting *t);
    DLL_EXPORT int test_buildmessages_step(ITesting *t);
    DLL_EXPORT int test_buildmessages_modalfilter(ITesting *t);
    DLL_EXPORT int test_buildmessages_keys(ITesting *t);
}

namespace {
    std::string ReadLog(const std::string &name) {
        std::ifstream f("testfiles/build-logs/" + name, std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    bool HasDetail(const BuildMessage &message, const std::string &needle) {
        for (auto &line : message.detail) {
            if (line.find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    BuildOutputParser ParseAll(const std::string &text, size_t chunk = 0) {
        BuildOutputParser parser;
        if (chunk == 0) {
            parser.Feed(text);
        } else {
            for (size_t i = 0; i < text.size(); i += chunk) {
                parser.Feed(std::string_view(text).substr(i, chunk));
            }
        }
        parser.Finish();
        return parser;
    }
}

DLL_EXPORT int test_buildmessages(ITesting *t) {
    return kTR_Pass;
}

// clang's colours stripped; the header's warning once though two files include it; notes and the include chain
// belong to their message; make's directory recorded
DLL_EXPORT int test_buildmessages_clangmake(ITesting *t) {
    auto log = ReadLog("clang-make.log");
    TR_ASSERT(t, !log.empty());
    auto parser = ParseAll(log);
    auto &messages = parser.Messages();
    TR_ASSERT(t, messages.size() == 4);
    TR_ASSERT(t, parser.NumErrors() == 3);
    TR_ASSERT(t, parser.NumWarnings() == 1);

    auto &warning = messages[0];
    TR_ASSERT(t, warning.severity == kSeverity::kWarning);
    TR_ASSERT(t, warning.path == "./util.h");
    TR_ASSERT(t, warning.line == 2);
    TR_ASSERT(t, warning.column == 39);
    TR_ASSERT(t, warning.message == "unused variable 'y' [-Wunused-variable]");
    TR_ASSERT(t, warning.directory == "/work/proj/sub");
    TR_ASSERT(t, !warning.detail.empty());
    TR_ASSERT(t, warning.detail[0] == "In file included from a.cpp:1:");

    TR_ASSERT(t, messages[1].path == "a.cpp");
    TR_ASSERT(t, messages[1].line == 3);
    TR_ASSERT(t, messages[1].message == "use of undeclared identifier 'undefined_name'");
    TR_ASSERT(t, HasDetail(messages[1], "return a + undefined_name;"));

    auto &ambiguous = messages[2];
    TR_ASSERT(t, ambiguous.path == "b.cpp");
    TR_ASSERT(t, ambiguous.line == 4);
    TR_ASSERT(t, ambiguous.column == 14);
    TR_ASSERT(t, HasDetail(ambiguous, "b.cpp:2:6: note: candidate function"));
    TR_ASSERT(t, HasDetail(ambiguous, "b.cpp:3:6: note: candidate function"));
    // The repeated header warning's include chain went with it, not onto this one
    TR_ASSERT(t, !HasDetail(ambiguous, "In file included from"));

    TR_ASSERT(t, messages[3].line == 4);
    TR_ASSERT(t, messages[3].column == 29);
    TR_ASSERT(t, messages[3].message == "expected ';' after return statement");
    for (auto &message : messages) {
        TR_ASSERT(t, message.message.find('\x1b') == std::string::npos);
    }
    return kTR_Pass;
}

// gcc's 'In function' context lines, ninja's FAILED noise, ld's undefined reference, the linker's own error with no place
DLL_EXPORT int test_buildmessages_gccninja(ITesting *t) {
    auto parser = ParseAll(ReadLog("gcc-ninja.log"));
    auto &messages = parser.Messages();
    TR_ASSERT(t, messages.size() == 5);
    TR_ASSERT(t, parser.NumErrors() == 4);

    TR_ASSERT(t, messages[0].path == "../src/util.h");
    TR_ASSERT(t, messages[0].severity == kSeverity::kWarning);
    TR_ASSERT(t, messages[0].detail.size() >= 2);
    TR_ASSERT(t, messages[0].detail[0] == "In file included from ../src/b.cpp:1:");
    TR_ASSERT(t, messages[0].detail[1] == "../src/util.h: In function 'int helper(int)':");

    TR_ASSERT(t, messages[1].path == "../src/b.cpp");
    TR_ASSERT(t, messages[1].message == "call of overloaded 'g(unsigned int)' is ambiguous");
    TR_ASSERT(t, messages[1].detail[0] == "../src/b.cpp: In function 'int main()':");
    TR_ASSERT(t, HasDetail(messages[1], "note: candidate: 'void g(long int)'"));

    TR_ASSERT(t, messages[2].column == 29);
    TR_ASSERT(t, HasDetail(messages[2], ";"));

    // The linker's 'undefined reference' (#52 step 4): no -g, so the file without a line
    TR_ASSERT(t, messages[3].path == "a.cpp");
    TR_ASSERT(t, messages[3].message == "undefined reference to `undefined_name'");
    TR_ASSERT(t, HasDetail(messages[3], "in function `f(int)'"));

    TR_ASSERT(t, messages[4].path.empty());
    TR_ASSERT(t, messages[4].line == 0);
    TR_ASSERT(t, messages[4].message == "collect2: ld returned 1 exit status");
    return kTR_Pass;
}

// The message is on the indented line after the header; blank lines inside; not taken as a repeat
DLL_EXPORT int test_buildmessages_cmake(ITesting *t) {
    auto parser = ParseAll(ReadLog("cmake-configure.log"));
    auto &messages = parser.Messages();
    TR_ASSERT(t, messages.size() == 3);

    TR_ASSERT(t, messages[0].severity == kSeverity::kWarning);
    TR_ASSERT(t, messages[0].path == "CMakeLists.txt");
    TR_ASSERT(t, messages[0].line == 4);
    TR_ASSERT(t, messages[0].message == "a warning");
    TR_ASSERT(t, HasDetail(messages[0], "over two lines"));

    TR_ASSERT(t, messages[1].severity == kSeverity::kError);
    TR_ASSERT(t, messages[1].line == 3);
    TR_ASSERT(t, messages[1].message == "Cannot find source file:");
    TR_ASSERT(t, HasDetail(messages[1], "missing.cpp"));

    TR_ASSERT(t, messages[2].message == "No SOURCES given to target: t");

    // Without a place
    auto noPlace = ParseAll("CMake Error: The source directory \"/x\" does not exist.\n");
    TR_ASSERT(t, noPlace.Messages().size() == 1);
    TR_ASSERT(t, noPlace.Messages()[0].path.empty());
    TR_ASSERT(t, noPlace.Messages()[0].message == "The source directory \"/x\" does not exist.");
    return kTR_Pass;
}

// Any split of the bytes - inside an escape, between '\r' and '\n' - reads the same; a message is listed as soon
// as its line is complete
DLL_EXPORT int test_buildmessages_streaming(ITesting *t) {
    auto log = ReadLog("clang-make.log");
    auto whole = ParseAll(log);
    for (size_t chunk : {1, 2, 3, 7, 64}) {
        auto split = ParseAll(log, chunk);
        TR_ASSERT(t, split.Messages().size() == whole.Messages().size());
        for (size_t i = 0; i < whole.Messages().size(); i++) {
            TR_ASSERT(t, split.Messages()[i].message == whole.Messages()[i].message);
            TR_ASSERT(t, split.Messages()[i].detail == whole.Messages()[i].detail);
        }
    }

    BuildOutputParser parser;
    parser.Feed("a.cpp:3:16: \x1b[0;1;31mer");
    TR_ASSERT(t, parser.Messages().empty());
    parser.Feed("ror: \x1b[0mbad\r");
    TR_ASSERT(t, parser.Messages().empty());
    parser.Feed("\nb.cpp:1:1: warning: x");
    TR_ASSERT(t, parser.Messages().size() == 1);
    TR_ASSERT(t, parser.Messages()[0].message == "bad");
    TR_ASSERT(t, parser.Messages()[0].outputLine == 0);
    // The last line has no '\n' - read at the end
    parser.Finish();
    TR_ASSERT(t, parser.Messages().size() == 2);
    TR_ASSERT(t, parser.Messages()[1].outputLine == 1);
    TR_ASSERT(t, parser.NumLines() == 2);
    return kTR_Pass;
}

// ninja in a terminal redraws its progress line with '\r' (+ erase); what's left is what the terminal shows
// clang wraps a message to the terminal's width (clang-wrapped.log: -fmessage-length 60 / 50, real captures): a long
// path leaves 'warning:' at the end of the line and all of the text below; a short one breaks the text over lines
// indented by 6. Missed: only the one warning whose text wasn't wrapped was found in a narrow terminal panel.
DLL_EXPORT int test_buildmessages_wrapped(ITesting *t) {
    auto log = ReadLog("clang-wrapped.log");
    auto parser = ParseAll(log);
    auto &messages = parser.Messages();
    TR_ASSERT(t, messages.size() == 4);
    TR_ASSERT(t, parser.NumErrors() == 2);
    TR_ASSERT(t, parser.NumWarnings() == 2);
    for (size_t i : {0, 2}) {
        TR_ASSERT(t, messages[i].IsWarning());
        TR_ASSERT(t, messages[i].line == 2);
        TR_ASSERT(t, messages[i].column == 9);
        TR_ASSERT(t, messages[i].message == "unused variable 'unused_variable_with_a_long_name' [-Wunused-variable]");
        // The source and caret are still detail, not text
        TR_ASSERT(t, messages[i].detail.size() == 2);
        TR_ASSERT(t, HasDetail(messages[i], "2 |"));
    }
    for (size_t i : {1, 3}) {
        TR_ASSERT(t, messages[i].IsError());
        TR_ASSERT(t, messages[i].message == "use of undeclared identifier 'nope_this_identifier_is_not_declared'");
        TR_ASSERT(t, messages[i].detail.size() == 2);
    }
    TR_ASSERT(t, messages[0].path == "/work/proj/src/a/rather/long/directory/name/for/wrapping/bad.c");
    TR_ASSERT(t, messages[2].path == "bad.c");

    for (size_t chunk : {1, 5, 64}) {
        auto split = ParseAll(log, chunk);
        TR_ASSERT(t, split.Messages().size() == messages.size());
        for (size_t i = 0; i < messages.size(); i++) {
            TR_ASSERT(t, split.Messages()[i].message == messages[i].message);
            TR_ASSERT(t, split.Messages()[i].detail == messages[i].detail);
        }
    }

    // A repeat is told by its whole text: the same wrapped warning twice is one, two that differ only below the
    // break are two
    auto repeats = ParseAll("a.h:1:1: warning:\n      same text\n"
                            "a.h:1:1: warning:\n      same text\n"
                            "a.h:1:1: warning:\n      other text\n");
    TR_ASSERT(t, repeats.Messages().size() == 2);
    TR_ASSERT(t, repeats.Messages()[0].message == "same text");
    TR_ASSERT(t, repeats.Messages()[1].message == "other text");

    // Not wrapped: a source line with clang's / gcc's gutter, an indent other than 6
    auto plain = ParseAll("a.c:1:1: warning: done\n    1 | int x;\n      |     ^\n"
                          "b.c:2:1: error: done too\n    int y;\n");
    TR_ASSERT(t, plain.Messages().size() == 2);
    TR_ASSERT(t, plain.Messages()[0].message == "done");
    TR_ASSERT(t, plain.Messages()[0].detail.size() == 2);
    TR_ASSERT(t, plain.Messages()[1].message == "done too");
    TR_ASSERT(t, plain.Messages()[1].detail.size() == 1);
    return kTR_Pass;
}

// trun's failures (trun-sequential.log / trun-forked.log: real captures, a test library with a failed assert, an Error,
// a Fatal, a case that just returned kTR_Fail). Each failure once though the summary at the end names it again; the
// case's name in the message ('_test_' on macOS, as its symbol)
DLL_EXPORT int test_buildmessages_trun(ITesting *t) {
    for (auto name : {"trun-sequential.log", "trun-forked.log"}) {
        auto log = ReadLog(name);
        for (size_t chunk : {0, 3}) {
            auto parser = ParseAll(log, chunk);
            auto &messages = parser.Messages();
            TR_ASSERT(t, messages.size() == 4);
            TR_ASSERT(t, parser.NumErrors() == 4);
            TR_ASSERT(t, messages[0].path == "src/deep/test_demo.cpp");
            TR_ASSERT(t, messages[0].line == 5);
            TR_ASSERT(t, messages[0].message == "test_demo_assert: a == 2");
            TR_ASSERT(t, messages[1].line == 6);
            TR_ASSERT(t, messages[1].message == "test_demo_error: the value was 42");
            TR_ASSERT(t, messages[2].line == 7);
            TR_ASSERT(t, messages[2].message == "test_demo_fatal: fatal thing");
            // Failed with no assert - counted, but no place to go to
            TR_ASSERT(t, messages[3].path.empty());
            TR_ASSERT(t, messages[3].message == "test_other_returnfail: failed");
            TR_ASSERT(t, BuildMessages::Order(messages).size() == 3);
        }
    }
    // Linux: no '_' in front; a failure before any '=== RUN' has no case to name
    auto parser = ParseAll("=== RUN  \ttest_x_y\n*** ASSERT ERROR: /src/a.cpp:12\t'b'\n*** ASSERT ERROR: /src/a.cpp:13\t'c'\n");
    TR_ASSERT(t, parser.Messages().size() == 2);
    TR_ASSERT(t, parser.Messages()[0].message == "test_x_y: b");
    auto bare = ParseAll("*** GENERAL ERROR: /src/a.cpp:3\t'no case'\n");
    TR_ASSERT(t, bare.Messages().size() == 1);
    TR_ASSERT(t, bare.Messages()[0].message == "no case");
    // Not trun's: a list item that only looks like its summary
    TR_ASSERT(t, ParseAll("  [abc]: something, else\n").Messages().empty());
    return kTR_Pass;
}

DLL_EXPORT int test_buildmessages_carriagereturn(ITesting *t) {
    auto parser = ParseAll("[1/3] Building a.o\r[2/3] Building b.o\x1b[K\r\x1b[K../src/b.cpp:4:29: error: expected ';'\r\n"
                           "\x1b]0;title\x07[3/3] Linking\n");
    TR_ASSERT(t, parser.Messages().size() == 1);
    TR_ASSERT(t, parser.Messages()[0].path == "../src/b.cpp");
    TR_ASSERT(t, parser.Messages()[0].message == "expected ';'");
    TR_ASSERT(t, parser.NumLines() == 2);
    return kTR_Pass;
}

// A Python run (a real capture, python3 -W always): a warning, then an exception raised while handling another. Each
// traceback is one error at the program's own last frame - not the json module's - with the frames as its detail
DLL_EXPORT int test_buildmessages_python(ITesting *t) {
    auto text = ReadLog("python-traceback.log");
    TR_ASSERT(t, !text.empty());
    for (size_t chunk : {(size_t)0, (size_t)7}) {
        auto parser = ParseAll(text, chunk);
        auto &messages = parser.Messages();
        TR_ASSERT(t, messages.size() == 3);
        TR_ASSERT(t, messages[0].IsWarning());
        TR_ASSERT(t, messages[0].path == "/work/py/main.py");
        TR_ASSERT(t, messages[0].line == 5);
        TR_ASSERT(t, messages[0].message == "DeprecationWarning: old api");
        TR_ASSERT(t, HasDetail(messages[0], "warnings.warn("));

        TR_ASSERT(t, messages[1].IsError());
        TR_ASSERT(t, messages[1].path == "/work/py/lib.py");
        TR_ASSERT(t, messages[1].line == 2);
        TR_ASSERT(t, messages[1].message == "ZeroDivisionError: division by zero");
        TR_ASSERT(t, HasDetail(messages[1], "divide(1, 0)"));

        // The last frames are in the json module: the place is main.py's call into it
        TR_ASSERT(t, messages[2].IsError());
        TR_ASSERT(t, messages[2].path == "/work/py/main.py");
        TR_ASSERT(t, messages[2].line == 8);
        TR_ASSERT(t, messages[2].message.find("json.decoder.JSONDecodeError: ") == 0);
        TR_ASSERT(t, HasDetail(messages[2], "json/decoder.py"));
        TR_ASSERT(t, parser.NumErrors() == 2);
        TR_ASSERT(t, parser.NumWarnings() == 1);
    }
    // Only library frames: the last one, not nothing
    auto parser = ParseAll("Traceback (most recent call last):\n"
                           "  File \"<frozen runpy>\", line 198, in _run_module_as_main\n"
                           "  File \"/usr/lib/python3.12/runpy.py\", line 88, in _run_code\n"
                           "KeyboardInterrupt\n");
    TR_ASSERT(t, parser.Messages().size() == 1);
    TR_ASSERT(t, parser.Messages()[0].path == "/usr/lib/python3.12/runpy.py");
    TR_ASSERT(t, parser.Messages()[0].message == "KeyboardInterrupt");
    return kTR_Pass;
}

// A SyntaxError has no 'Traceback' line - the File line, the source, the caret, the error
DLL_EXPORT int test_buildmessages_pythonsyntax(ITesting *t) {
    auto parser = ParseAll(ReadLog("python-syntax.log"));
    auto &messages = parser.Messages();
    TR_ASSERT(t, messages.size() == 1);
    TR_ASSERT(t, messages[0].IsError());
    TR_ASSERT(t, messages[0].path == "/work/py/bad.py");
    TR_ASSERT(t, messages[0].line == 2);
    TR_ASSERT(t, messages[0].message == "SyntaxError: '(' was never closed");
    // A File line that isn't followed by an exception (a blank line, the prompt) lists nothing
    TR_ASSERT(t, ParseAll("  File \"x.py\", line 3\n\n$ ls\n").Messages().empty());
    return kTR_Pass;
}

// cargo build (real captures, older rustc): the place is on the ' --> ' line under the message; cargo's own summaries
// ('could not compile', 'generated 1 warning') are not messages
DLL_EXPORT int test_buildmessages_cargo(ITesting *t) {
    auto text = ReadLog("cargo.log");
    TR_ASSERT(t, !text.empty());
    for (size_t chunk : {(size_t)0, (size_t)5}) {
        auto parser = ParseAll(text, chunk);
        auto &messages = parser.Messages();
        TR_ASSERT(t, parser.NumErrors() == 3);
        TR_ASSERT(t, parser.NumWarnings() == 1);
        TR_ASSERT(t, messages.size() == 4);
        TR_ASSERT(t, messages[0].path == "src/main.rs");
        TR_ASSERT(t, messages[0].line == 4);
        TR_ASSERT(t, messages[0].column == 20);
        TR_ASSERT(t, messages[0].message == "cannot find value `z` in this scope [E0425]");
        TR_ASSERT(t, HasDetail(messages[0], "println!"));
        TR_ASSERT(t, messages[1].line == 3);
        TR_ASSERT(t, messages[1].message == "mismatched types [E0308]");
        // The coloured one: escapes stripped
        TR_ASSERT(t, messages[2].path == "src/main.rs");
        TR_ASSERT(t, messages[2].line == 3);
        TR_ASSERT(t, messages[3].IsWarning());
        TR_ASSERT(t, messages[3].message == "unused variable: `x`");
        TR_ASSERT(t, messages[3].line == 2);
        TR_ASSERT(t, messages[3].column == 9);
        TR_ASSERT(t, HasDetail(messages[3], "= note:"));
    }
    return kTR_Pass;
}

// GNU ld (written from its format - no GNU ld on the Mac) and lld: the source line when -g gave one, the object's
// messages without a place, lld's place on its '>>> referenced by' line
DLL_EXPORT int test_buildmessages_linker(ITesting *t) {
    auto parser = ParseAll(ReadLog("ld.log"));
    auto &messages = parser.Messages();
    TR_ASSERT(t, messages.size() == 7);
    TR_ASSERT(t, messages[0].path == "/work/app/main.cpp");
    TR_ASSERT(t, messages[0].line == 5);
    TR_ASSERT(t, messages[0].message == "undefined reference to `helper()'");
    TR_ASSERT(t, HasDetail(messages[0], "in function `main'"));
    TR_ASSERT(t, messages[1].path == "util.cpp");
    TR_ASSERT(t, messages[1].line == 0);
    TR_ASSERT(t, messages[2].path == "/work/app/b.cpp");
    TR_ASSERT(t, messages[2].line == 1);
    TR_ASSERT(t, messages[2].message.find("multiple definition of `counter'") == 0);
    // In an archive's object: no place
    TR_ASSERT(t, messages[3].path.empty());
    TR_ASSERT(t, messages[4].path.empty());     // collect2
    TR_ASSERT(t, messages[5].path == "/work/app/main.cpp");
    TR_ASSERT(t, messages[5].line == 7);
    TR_ASSERT(t, messages[5].message == "ld.lld: undefined symbol: foo()");
    TR_ASSERT(t, messages[6].path.empty());     // clang++: linker command failed
    TR_ASSERT(t, parser.NumErrors() == 7);
    return kTR_Pass;
}

// A sentence that holds ': error: ' is not a message; a tool's own line is; 'file:line:' without a column is
DLL_EXPORT int test_buildmessages_noplace(ITesting *t) {
    auto parser = ParseAll("this is a sentence: error: not a message\n"
                           "clang: error: linker command failed with exit code 1\n"
                           "src/old.c:12: warning: old-style definition\n"
                           "  indented: error: also not\n");
    auto &messages = parser.Messages();
    TR_ASSERT(t, messages.size() == 2);
    TR_ASSERT(t, messages[0].path.empty());
    TR_ASSERT(t, messages[0].message == "clang: linker command failed with exit code 1");
    TR_ASSERT(t, messages[1].path == "src/old.c");
    TR_ASSERT(t, messages[1].line == 12);
    TR_ASSERT(t, messages[1].column == 0);
    return kTR_Pass;
}

// make -j: directories interleave; each message gets the innermost one entered when it was printed
DLL_EXPORT int test_buildmessages_makedirs(ITesting *t) {
    auto parser = ParseAll("make[1]: Entering directory '/p/a'\n"
                           "make[1]: Entering directory '/p/b'\n"
                           "x.c:1:1: error: in b\n"
                           "make[1]: Leaving directory '/p/a'\n"
                           "y.c:1:1: error: still in b\n"
                           "make[1]: Leaving directory '/p/b'\n"
                           "z.c:1:1: error: nowhere\n"
                           "make[2]: Entering directory \xE2\x80\x98/p/c\xE2\x80\x99\n"
                           "w.c:1:1: error: localised quotes\n");
    auto &messages = parser.Messages();
    TR_ASSERT(t, messages.size() == 4);
    TR_ASSERT(t, messages[0].directory == "/p/b");
    TR_ASSERT(t, messages[1].directory == "/p/b");
    TR_ASSERT(t, messages[2].directory.empty());
    TR_ASSERT(t, messages[3].directory == "/p/c");
    return kTR_Pass;
}

DLL_EXPORT int test_buildmessages_reset(ITesting *t) {
    BuildOutputParser parser;
    parser.Feed("a.c:1:1: error: x\n");
    parser.Reset();
    parser.Feed("a.c:1:1: error: x\n");
    // Not a repeat of the last run's
    TR_ASSERT(t, parser.Messages().size() == 1);
    TR_ASSERT(t, parser.NumLines() == 1);
    return kTR_Pass;
}

// The commands we read (§9.2): the tool's name, wherever it is, after assignments and 'time'
DLL_EXPORT int test_buildmessages_commands(ITesting *t) {
    for (auto cmd : {"make", "make -j8", "cmake --build cmake-build-debug", "ninja -C build", "/usr/bin/make all",
                     "CC=clang make", "time make", "env CXX=g++ ninja", "clang++ -c a.cpp", "g++-13 a.cpp", "  gcc x.c",
                     "trun --sequential ./libutests.dylib", "/usr/local/bin/trun -m x lib.so", "cargo build",
                     "python3 main.py", "python3.12 -m pytest", "python game.py", "pytest -x", "rustc a.rs"}) {
        TR_ASSERT(t, BuildMessages::IsBuildCommand(cmd));
    }
    for (auto cmd : {"", "ls", "ls make", "git commit -m make", "./build.sh", "makeself", "cat Makefile", "time", "python3-config", "pythonista"}) {
        TR_ASSERT(t, !BuildMessages::IsBuildCommand(cmd));
    }
    return kTR_Pass;
}

// Several terminals (hitlist #53): a run belongs to the one it was typed in - another one's output (an agent's) and its
// 'ls' leave it alone; another one's build replaces it
DLL_EXPORT int test_buildmessages_terminals(ITesting *t) {
    int buildShell = 0;
    int agentShell = 0;
    BuildMessages build;
    TR_ASSERT(t, build.OnCommand("make", "/w", "/w", &buildShell));
    build.Feed("a.c:1:1: error: one\n", &buildShell);
    build.Feed("b.c:2:1: error: printed by the agent\n", &agentShell);
    TR_ASSERT(t, !build.OnCommand("ls", "/w", "/w", &agentShell));
    TR_ASSERT(t, build.IsRunning());
    TR_ASSERT(t, !build.UpdateJobState(false, &agentShell));
    build.Feed("c.c:3:1: error: two\n", &buildShell);
    TR_ASSERT(t, build.NumErrors() == 2);
    TR_ASSERT(t, build.UpdateJobState(true, &buildShell) == false);
    TR_ASSERT(t, build.UpdateJobState(false, &buildShell));
    TR_ASSERT(t, !build.IsRunning());

    // The agent's terminal builds: its run now
    TR_ASSERT(t, build.OnCommand("ninja", "/w", "/w", &agentShell));
    build.Feed("d.c:4:1: warning: x\n", &agentShell);
    build.Feed("e.c:5:1: error: from the other one\n", &buildShell);
    TR_ASSERT(t, (build.NumErrors() == 0) && (build.NumWarnings() == 1));
    return kTR_Pass;
}

// A build starts a run; the bytes after it are read; an 'ls' after it ends the run but keeps its list; the next
// build starts over
DLL_EXPORT int test_buildmessages_run(ITesting *t) {
    BuildMessages build;
    TR_ASSERT(t, !build.HasRun());
    build.Feed("a.c:1:1: error: before any run\n");
    TR_ASSERT(t, build.Messages().empty());

    TR_ASSERT(t, build.OnCommand("make", "/p", "/p"));
    TR_ASSERT(t, build.IsRunning());
    build.Feed("a.c:1:1: error: one\nb.c:2:1: warn");
    TR_ASSERT(t, build.Messages().size() == 1);     // live: there while it runs
    build.Feed("ing: two\n");
    TR_ASSERT(t, build.NumErrors() == 1);
    TR_ASSERT(t, build.NumWarnings() == 1);
    TR_ASSERT(t, build.Summary() == "1 error, 1 warning");

    TR_ASSERT(t, !build.OnCommand("ls", "/p", "/p"));
    TR_ASSERT(t, !build.IsRunning());
    build.Feed("c.c:3:1: error: ls doesn't print this\n");
    TR_ASSERT(t, build.Messages().size() == 2);
    TR_ASSERT(t, build.Command() == "make");

    TR_ASSERT(t, build.OnCommand("make", "/p", "/p"));
    TR_ASSERT(t, build.Messages().empty());
    TR_ASSERT(t, build.Summary() == "no errors or warnings");
    return kTR_Pass;
}

// The run ends with its job - only once a job was seen (the bytes can come before the job is in the foreground)
DLL_EXPORT int test_buildmessages_jobend(ITesting *t) {
    BuildMessages build;
    build.OnCommand("make", "", "");
    TR_ASSERT(t, !build.UpdateJobState(false));
    TR_ASSERT(t, build.IsRunning());
    TR_ASSERT(t, !build.UpdateJobState(true));
    build.Feed("a.c:1:1: error: last line, no newline");
    TR_ASSERT(t, build.UpdateJobState(false));
    TR_ASSERT(t, !build.IsRunning());
    // The last line was read at the end
    TR_ASSERT(t, build.Messages().size() == 1);
    TR_ASSERT(t, !build.UpdateJobState(false));
    return kTR_Pass;
}

// The shell's 'command finished' (OSC 133;D) ends the run even when the job was never seen running - a fast cc
DLL_EXPORT int test_buildmessages_commanddone(ITesting *t) {
    int buildShell = 0;
    int otherShell = 0;
    BuildMessages build;
    build.OnCommand("cc -c bad.c", "", "", &buildShell);
    build.Feed("bad.c:1:1: error: one\n", &buildShell);
    TR_ASSERT(t, !build.UpdateJobState(false, &buildShell));
    TR_ASSERT(t, build.IsRunning());
    // Another terminal's command ending is not this run's
    TR_ASSERT(t, !build.OnCommandDone(&otherShell));
    TR_ASSERT(t, build.IsRunning());
    TR_ASSERT(t, build.OnCommandDone(&buildShell));
    TR_ASSERT(t, !build.IsRunning());
    TR_ASSERT(t, build.NumErrors() == 1);
    TR_ASSERT(t, !build.OnCommandDone(&buildShell));
    return kTR_Pass;
}

// The terminal went quiet: a job running is seen (the run goes on), none ends the run - also one never seen running
DLL_EXPORT int test_buildmessages_quiet(ITesting *t) {
    int buildShell = 0;
    int otherShell = 0;
    BuildMessages build;
    build.OnCommand("make", "", "", &buildShell);
    build.Feed("a.c:1:1: error: one\n", &buildShell);
    TR_ASSERT(t, !build.OnQuiet(true, &buildShell));
    TR_ASSERT(t, build.IsRunning());
    TR_ASSERT(t, !build.OnQuiet(false, &otherShell));
    TR_ASSERT(t, build.IsRunning());
    TR_ASSERT(t, build.OnQuiet(false, &buildShell));
    TR_ASSERT(t, !build.IsRunning());

    // Never seen running (UpdateJobState can't end it) - quiet does
    build.OnCommand("cc -c bad.c", "", "", &buildShell);
    build.Feed("bad.c:1:1: error: one\n", &buildShell);
    TR_ASSERT(t, !build.UpdateJobState(false, &buildShell));
    TR_ASSERT(t, build.OnQuiet(false, &buildShell));
    TR_ASSERT(t, build.NumErrors() == 1);
    return kTR_Pass;
}

// Relative paths: make's directory first, then the build directory the command named, the shell's, the project root
DLL_EXPORT int test_buildmessages_resolve(ITesting *t) {
    auto root = fs::canonical(fs::temp_directory_path()) / "goatedit_buildmessages_resolve";
    fs::remove_all(root);
    fs::create_directories(root / "src" / "sub");
    fs::create_directories(root / "build");
    std::ofstream(root / "src" / "a.cpp") << "x\n";
    std::ofstream(root / "src" / "sub" / "b.cpp") << "x\n";
    std::ofstream(root / "top.cpp") << "x\n";

    BuildMessages build;
    build.OnCommand("cmake --build build", (root / "src").string(), root.string());
    BuildMessage message;
    message.path = "a.cpp";
    TR_ASSERT(t, build.ResolvePath(message) == (root / "src" / "a.cpp").string());       // the shell's directory
    message.path = "../src/a.cpp";
    TR_ASSERT(t, build.ResolvePath(message) == (root / "src" / "a.cpp").string());       // build/../src
    message.path = "b.cpp";
    message.directory = (root / "src" / "sub").string();
    TR_ASSERT(t, build.ResolvePath(message) == (root / "src" / "sub" / "b.cpp").string());  // make's
    message.directory = "sub";                                                             // ninja's, relative
    TR_ASSERT(t, build.ResolvePath(message) == (root / "src" / "sub" / "b.cpp").string());
    message.directory.clear();
    message.path = "top.cpp";
    TR_ASSERT(t, build.ResolvePath(message) == (root / "top.cpp").string());              // the project root
    message.path = (root / "top.cpp").string();
    TR_ASSERT(t, build.ResolvePath(message) == (root / "top.cpp").string());
    message.path = "nosuch.cpp";
    TR_ASSERT(t, build.ResolvePath(message).empty());
    fs::remove_all(root);
    return kTR_Pass;
}

// Next / previous step over messages with a place - errors first, then warnings (as the list) - and wrap around
DLL_EXPORT int test_buildmessages_step(ITesting *t) {
    BuildMessages build;
    TR_ASSERT(t, !build.Next().has_value());
    build.OnCommand("make", "", "");
    build.Feed("a.c:1:1: error: first\n"
               "CMake Error: no place\n"
               "b.c:2:1: warning: second\n"
               "c.c:3:1: error: third\n");
    auto first = build.Next();
    TR_ASSERT(t, first.has_value());
    TR_ASSERT(t, first->message.message == "first");
    TR_ASSERT(t, first->index == 0);
    TR_ASSERT(t, first->count == 3);
    auto third = build.Next();
    TR_ASSERT(t, third->message.message == "third");
    TR_ASSERT(t, third->index == 1);
    TR_ASSERT(t, build.Next()->message.message == "second");     // the warning after the errors
    TR_ASSERT(t, build.Next()->message.message == "first");      // around
    TR_ASSERT(t, build.Previous()->message.message == "second");
    TR_ASSERT(t, build.Previous()->index == 1);

    // Previous from nothing selected: the last one
    BuildMessages other;
    other.OnCommand("make", "", "");
    other.Feed("a.c:1:1: error: first\nb.c:2:1: error: last\n");
    TR_ASSERT(t, other.Previous()->message.message == "last");
    // A pick in the list: F8 goes on from it
    TR_ASSERT(t, other.Select(0).has_value());
    TR_ASSERT(t, other.Next()->message.message == "last");
    TR_ASSERT(t, !other.Select(5).has_value());
    return kTR_Pass;
}

// Errors first, then warnings; no notes, nothing without a file; a query filters on the row
DLL_EXPORT int test_buildmessages_modalfilter(ITesting *t) {
    BuildOutputParser parser;
    parser.Feed("src/a.c:1:2: warning: unused x\n"
                "src/b.c:3:4: error: bad thing\n"
                "orphan.c:5:1: note: a note\n"
                "clang: error: linker command failed\n"
                "src/c.c:6:1: error: other\n");
    parser.Finish();
    auto &messages = parser.Messages();
    auto rows = BuildMessagesModal::Filter(messages, U"");
    TR_ASSERT(t, rows.size() == 3);
    TR_ASSERT(t, messages[rows[0]].message == "bad thing");
    TR_ASSERT(t, messages[rows[1]].message == "other");
    TR_ASSERT(t, messages[rows[2]].message == "unused x");
    TR_ASSERT(t, BuildMessagesModal::RowText(messages[rows[0]]) == U"error    b.c:3:4  bad thing");

    rows = BuildMessagesModal::Filter(messages, U"A.C");
    TR_ASSERT(t, rows.size() == 1);
    TR_ASSERT(t, messages[rows[0]].message == "unused x");
    TR_ASSERT(t, BuildMessagesModal::Filter(messages, U"nothing like it").empty());
    return kTR_Pass;
}

// F8 / Shift+F8 / Cmd(Ctrl)+F8 from the editor, the tree and the terminal
DLL_EXPORT int test_buildmessages_keys(ITesting *t) {
    KeyMappingCache::Instance().Clear();
    KeyPress keyPress = {};
    keyPress.isKeyValid = true;
    keyPress.isSpecialKey = true;
    keyPress.specialKey = Keyboard::kKeyCode_F8;
    keyPress.key = Keyboard::kKeyCode_F8;
#ifdef GEDIT_MACOS
    auto copyPaste = Keyboard::kMod_LeftCommand;
#else
    auto copyPaste = Keyboard::kMod_LeftCtrl;
#endif
    for (auto &keymapName : { "default_keymap", "workspace_keymap", "terminal_keymap" }) {
        auto keymap = Editor::Instance().GetKeyMapping(keymapName);
        TR_ASSERT(t, keymap != nullptr);
        keyPress.modifiers = 0;
        auto action = keymap->ActionFromKeyPress(keyPress);
        TR_ASSERT(t, action.has_value() && (action->action == kAction::kActionNextBuildMessage));
        keyPress.modifiers = Keyboard::kMod_LeftShift;
        action = keymap->ActionFromKeyPress(keyPress);
        TR_ASSERT(t, action.has_value() && (action->action == kAction::kActionPreviousBuildMessage));
        keyPress.modifiers = copyPaste;
        action = keymap->ActionFromKeyPress(keyPress);
        TR_ASSERT(t, action.has_value() && (action->action == kAction::kActionShowBuildMessages));
    }
    return kTR_Pass;
}

//
// Reads a build's output and picks out its errors and warnings - see BuildOutputParser.h, docs/build-messages.md §4
//

#include <algorithm>
#include <cctype>
#include <filesystem>

#include "BuildOutputParser.h"

using namespace gedit;
namespace fs = std::filesystem;

static bool StartsWith(std::string_view text, std::string_view prefix) {
    return text.substr(0, prefix.size()) == prefix;
}

static bool IsIndented(std::string_view line) {
    return !line.empty() && ((line[0] == ' ') || (line[0] == '\t'));
}

static std::string Trim(std::string_view text) {
    size_t start = 0;
    while ((start < text.size()) && std::isspace((unsigned char)text[start])) {
        start++;
    }
    size_t end = text.size();
    while ((end > start) && std::isspace((unsigned char)text[end - 1])) {
        end--;
    }
    return std::string(text.substr(start, end - start));
}

static bool IsAllDigits(std::string_view text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](char ch) { return std::isdigit((unsigned char)ch); });
}

// 'path:line:col', 'path:line' or 'path' - the numbers taken off the end
static void SplitLocation(std::string_view location, std::string &path, uint32_t &line, uint32_t &column) {
    uint32_t numbers[2] = {0, 0};
    int count = 0;
    while (count < 2) {
        auto colon = location.rfind(':');
        if ((colon == std::string_view::npos) || !IsAllDigits(location.substr(colon + 1))) {
            break;
        }
        numbers[count++] = (uint32_t)std::stoul(std::string(location.substr(colon + 1)));
        location = location.substr(0, colon);
    }
    path = std::string(location);
    line = (count == 2) ? numbers[1] : numbers[0];
    column = (count == 2) ? numbers[0] : 0;
}

// The first severity marker on the line is 'note:'
static bool IsNote(std::string_view line) {
    auto note = line.find(": note: ");
    if (note == std::string_view::npos) {
        return false;
    }
    for (auto marker : {": error: ", ": fatal error: ", ": warning: "}) {
        if (line.find(marker) < note) {
            return false;
        }
    }
    return true;
}

// A source line with clang's / gcc's line-number gutter ('  318 |   foo();') or its caret line ('      |   ^~~')
static bool IsSourceGutter(std::string_view line) {
    auto text = Trim(line);
    size_t digits = 0;
    while ((digits < text.size()) && std::isdigit((unsigned char)text[digits])) {
        digits++;
    }
    auto rest = Trim(std::string_view(text).substr(digits));
    return StartsWith(rest, "|");
}

// Clang wraps a message to the terminal's width (-fmessage-length follows the pty's columns): the rest comes on
// lines indented by 6 - all of it when the place alone filled the line ('a/long/path.cpp:3:4: warning:'). An empty
// message takes any indented line that isn't source.
static bool IsWrappedMessage(std::string_view line, bool isMessageEmpty) {
    if (!IsIndented(line) || IsSourceGutter(line)) {
        return false;
    }
    if (isMessageEmpty) {
        return true;
    }
    return StartsWith(line, "      ") && (line.size() > 6) && !std::isspace((unsigned char)line[6]);
}

// Take off the quotes make / ninja put around a directory: `x', 'x', and the UTF-8 ‘x’ of a localised make
static std::string Unquote(std::string_view text) {
    static const std::string_view openQuotes[] = {"`", "'", "\"", "\xE2\x80\x98"};
    static const std::string_view closeQuotes[] = {"'", "\"", "\xE2\x80\x99"};
    for (auto quote : openQuotes) {
        if (StartsWith(text, quote)) {
            text.remove_prefix(quote.size());
            break;
        }
    }
    for (auto quote : closeQuotes) {
        if ((text.size() >= quote.size()) && (text.substr(text.size() - quote.size()) == quote)) {
            text.remove_suffix(quote.size());
            break;
        }
    }
    return std::string(text);
}

void BuildOutputParser::Feed(std::string_view bytes) {
    for (auto ch : bytes) {
        AcceptByte(ch);
    }
}

void BuildOutputParser::Finish() {
    if (!current.empty()) {
        EndLine();
    }
    escState = kEscState::kText;
    isPendingCR = false;
    continuation = kContinuation::kNone;
    pendingContext.clear();
    isInPythonTraceback = false;
    pythonFrames.clear();
}

void BuildOutputParser::Reset() {
    messages.clear();
    seen.clear();
    numLines = 0;
    current.clear();
    escState = kEscState::kText;
    isPendingCR = false;
    directories.clear();
    pendingContext.clear();
    continuation = kContinuation::kNone;
    isSkippingRepeat = false;
    isCMakeAwaitingText = false;
    isMessageWrapOpen = false;
    trunCase.clear();
    isInPythonTraceback = false;
    pythonFrames.clear();
}

size_t BuildOutputParser::NumErrors() const {
    return std::count_if(messages.begin(), messages.end(), [](const BuildMessage &message) { return message.IsError(); });
}

size_t BuildOutputParser::NumWarnings() const {
    return std::count_if(messages.begin(), messages.end(), [](const BuildMessage &message) { return message.IsWarning(); });
}

// The byte stream -> complete lines. Escapes (CSI colours, OSC titles) are dropped; a '\r' not followed by '\n'
// starts the line over, as the terminal draws it (ninja's '[3/40] Building ...' progress line).
void BuildOutputParser::AcceptByte(char ch) {
    switch (escState) {
        case kEscState::kText :
            break;
        case kEscState::kEsc :
            if (ch == '[') {
                escState = kEscState::kCsi;
            } else if ((ch == ']') || (ch == 'P') || (ch == '_') || (ch == '^')) {
                escState = kEscState::kString;
            } else if ((ch < 0x20) || (ch > 0x2f)) {
                // Not an intermediate byte ('(' in 'ESC ( B'): the escape is complete
                escState = kEscState::kText;
            }
            return;
        case kEscState::kCsi :
            if ((ch >= 0x40) && (ch <= 0x7e)) {
                escState = kEscState::kText;
            }
            return;
        case kEscState::kString :
            if (ch == 0x07) {
                escState = kEscState::kText;
            } else if (ch == 0x1b) {
                escState = kEscState::kStringEsc;
            }
            return;
        case kEscState::kStringEsc :
            escState = (ch == '\\') ? kEscState::kText : kEscState::kString;
            return;
    }

    if (ch == '\n') {
        isPendingCR = false;
        EndLine();
        return;
    }
    if (ch == '\r') {
        isPendingCR = true;
        return;
    }
    if (isPendingCR) {
        current.clear();
        isPendingCR = false;
    }
    if (ch == 0x1b) {
        escState = kEscState::kEsc;
        return;
    }
    if (((unsigned char)ch < 0x20) && (ch != '\t')) {
        return;
    }
    current.push_back(ch);
}

void BuildOutputParser::EndLine() {
    auto end = current.find_last_not_of(" \t");
    current.resize((end == std::string::npos) ? 0 : end + 1);
    ParseLine(current);
    current.clear();
    numLines++;
}

void BuildOutputParser::ParseLine(const std::string &line) {
    if (ParseDirectory(line)) {
        continuation = kContinuation::kNone;
        return;
    }
    // A traceback's lines are its own - also right after a message whose continuation would take indented lines
    if (ParsePython(line)) {
        continuation = kContinuation::kNone;
        return;
    }
    if ((continuation != kContinuation::kNone) && ParseContinuation(line)) {
        return;
    }
    continuation = kContinuation::kNone;
    isSkippingRepeat = false;
    if (ParseTrun(line) || ParseCMake(line) || ParseRust(line) || ParsePythonWarning(line) || ParseLinker(line) ||
        ParseCompiler(line) || ParseIncludeContext(line)) {
        return;
    }
    pendingContext.clear();
}

// 'make[2]: Entering directory '/x'', 'ninja: Entering directory `build'' - relative paths are relative to it
bool BuildOutputParser::ParseDirectory(const std::string &line) {
    static const std::string_view entering = ": Entering directory ";
    static const std::string_view leaving = ": Leaving directory ";
    auto pos = line.find(entering);
    bool isEntering = (pos != std::string::npos);
    if (!isEntering) {
        pos = line.find(leaving);
        if (pos == std::string::npos) {
            return false;
        }
    }
    auto tool = std::string_view(line).substr(0, pos);
    if ((tool.find("make") == std::string_view::npos) && (tool.find("ninja") == std::string_view::npos)) {
        return false;
    }
    auto directory = Unquote(std::string_view(line).substr(pos + (isEntering ? entering.size() : leaving.size())));
    if (isEntering) {
        directories.push_back(directory);
        return true;
    }
    // With -j the directories interleave: leave the one named, wherever it is
    auto it = std::find(directories.rbegin(), directories.rend(), directory);
    if (it != directories.rend()) {
        directories.erase(std::next(it).base());
    } else if (!directories.empty()) {
        directories.pop_back();
    }
    return true;
}

// The lines after a message that belong to it. True when the line was taken.
bool BuildOutputParser::ParseContinuation(const std::string &line) {
    auto addDetail = [this](std::string text) {
        if (!isSkippingRepeat && !messages.empty()) {
            messages.back().detail.push_back(std::move(text));
        }
    };

    if (continuation == kContinuation::kCompiler) {
        if (isMessageWrapOpen && IsWrappedMessage(line, isSkippingRepeat || messages.empty() || messages.back().message.empty())) {
            AppendWrapped(Trim(line));
            return true;
        }
        isMessageWrapOpen = false;
        // The source line and its caret ('   12 |   foo();' / '      |   ^~~'), clang's quoted source
        if (IsIndented(line)) {
            addDetail(line);
            return true;
        }
        // 'note:' with a place of its own belongs to the message above
        if (IsNote(line)) {
            addDetail(line);
            return true;
        }
        // lld: '>>> referenced by main.cpp:7 (/work/main.cpp:7)' - the place of an 'undefined symbol'
        if (StartsWith(line, ">>> ")) {
            ParseLinkerReference(line);
            addDetail(line);
            return true;
        }
        return false;
    }

    if (continuation == kContinuation::kRust) {
        auto text = Trim(line);
        // ' --> src/main.rs:4:20' - the message's place; a later one (a note's) is only detail
        if (StartsWith(text, "--> ")) {
            if (!isSkippingRepeat && !messages.empty() && messages.back().path.empty()) {
                auto &message = messages.back();
                SplitLocation(Trim(std::string_view(text).substr(4)), message.path, message.line, message.column);
            }
            addDetail(line);
            return true;
        }
        // The source with its gutter ('4 |     println!', '  |  ^'), '  = note:', a note / help of its own, '...'
        if (IsIndented(line) || IsSourceGutter(line) || StartsWith(text, "|") || StartsWith(line, "note: ") ||
            StartsWith(line, "help: ") || (text == "...")) {
            addDetail(line);
            return true;
        }
        return false;
    }

    // CMake: the message is on the indented lines after the header, blank lines inside; then 'Call Stack ...'
    if (line.empty()) {
        return true;
    }
    if (IsIndented(line)) {
        auto text = Trim(line);
        if (isCMakeAwaitingText && !messages.empty()) {
            messages.back().message = text;
            isCMakeAwaitingText = false;
        } else {
            addDetail(text);
        }
        return true;
    }
    if (StartsWith(line, "Call Stack (most recent call first):")) {
        addDetail(line);
        return true;
    }
    isCMakeAwaitingText = false;
    return false;
}

// trun (gnilk/testrunner), sequential or forked - the case, then its failure; the summary at the end names them again:
//   === RUN  \t_test_demo_assert
//   *** ASSERT ERROR: src/test_demo.cpp:5\t'a == 2'          (also GENERAL ERROR, FATAL ERROR)
//   [Tma]: _test_demo_assert, src/test_demo.cpp:5, a == 2    (the summary - the same message, so a repeat)
//   [Tma]: _test_other_returnfail,                           (failed with no assert: no place)
bool BuildOutputParser::ParseTrun(const std::string &line) {
    // macOS' symbols have a leading '_'
    auto caseName = [](std::string_view text) {
        auto name = Trim(text);
        return StartsWith(name, "_test_") ? name.substr(1) : name;
    };
    auto unquote = [](std::string text) {
        if ((text.size() >= 2) && (text.front() == '\'') && (text.back() == '\'')) {
            return text.substr(1, text.size() - 2);
        }
        return text;
    };
    auto add = [this](const std::string &name, std::string_view location, const std::string &text) {
        BuildMessage message;
        SplitLocation(Trim(location), message.path, message.line, message.column);
        if (message.line == 0) {
            message.path.clear();
        }
        message.message = name.empty() ? text : name + ": " + (text.empty() ? std::string("failed") : text);
        Add(std::move(message), kContinuation::kNone, true);
    };

    if (StartsWith(line, "=== RUN")) {
        trunCase = caseName(std::string_view(line).substr(7));
        return true;
    }
    for (auto marker : {"*** ASSERT ERROR: ", "*** GENERAL ERROR: ", "*** FATAL ERROR: "}) {
        if (!StartsWith(line, marker)) {
            continue;
        }
        auto rest = std::string_view(line).substr(std::string_view(marker).size());
        auto tab = rest.find('\t');
        auto location = rest.substr(0, tab);
        auto text = (tab == std::string_view::npos) ? std::string() : unquote(Trim(rest.substr(tab + 1)));
        add(trunCase, location, text);
        return true;
    }
    // '  [Tma]: name, place, message'
    if ((line.size() > 9) && StartsWith(line, "  [") && (line.compare(6, 3, "]: ") == 0)) {
        // The line's trailing blank is gone: '_test_x,' when there is no place
        auto text = line.substr(9) + " ";
        auto rest = std::string_view(text);
        auto comma = rest.find(", ");
        auto name = caseName(rest.substr(0, comma));
        if ((comma == std::string_view::npos) || !StartsWith(name, "test_")) {
            return false;
        }
        rest = rest.substr(comma + 2);
        comma = rest.find(", ");
        auto location = rest.substr(0, comma);
        add(name, location, (comma == std::string_view::npos) ? std::string() : Trim(rest.substr(comma + 2)));
        return true;
    }
    return false;
}

// Python's traceback - the frames, then the exception; one message at the frame that is the program's own (the last
// one not in Python's library or an installed package), the frames as its detail:
//   Traceback (most recent call last):
//     File "/work/main.py", line 11, in <module>
//       divide(1, 0)
//     File "/work/lib.py", line 2, in divide
//   ZeroDivisionError: division by zero
// A SyntaxError has no 'Traceback' line, only the 'File' line, the source and the caret.
bool BuildOutputParser::ParsePython(const std::string &line) {
    static const std::string_view fileMarker = "  File \"";
    static const std::string_view lineMarker = "\", line ";
    if (line == "Traceback (most recent call last):") {
        isInPythonTraceback = true;
        pythonFrames.clear();
        return true;
    }
    if (StartsWith(line, fileMarker)) {
        auto rest = std::string_view(line).substr(fileMarker.size());
        auto quote = rest.find(lineMarker);
        if (quote != std::string_view::npos) {
            auto number = rest.substr(quote + lineMarker.size());
            number = number.substr(0, number.find(','));
            if (IsAllDigits(number)) {
                PythonFrame frame;
                frame.path = std::string(rest.substr(0, quote));
                frame.line = (uint32_t)std::stoul(std::string(number));
                frame.lines.push_back(line);
                pythonFrames.push_back(std::move(frame));
                isInPythonTraceback = true;
                return true;
            }
        }
    }
    if (!isInPythonTraceback) {
        return false;
    }
    // The source under a frame, its carets, '[Previous line repeated 996 more times]'
    if (IsIndented(line)) {
        if (!pythonFrames.empty()) {
            pythonFrames.back().lines.push_back(line);
        }
        return true;
    }
    isInPythonTraceback = false;
    if (line.empty() || pythonFrames.empty()) {
        pythonFrames.clear();
        return false;
    }
    // The exception: 'ZeroDivisionError: division by zero', 'KeyboardInterrupt'
    auto isLibrary = [](const std::string &path) {
        return path.empty() || (path[0] == '<') || (path.find("/lib/python") != std::string::npos) ||
               (path.find("site-packages") != std::string::npos) || (path.find("\\Lib\\") != std::string::npos);
    };
    auto it = std::find_if(pythonFrames.rbegin(), pythonFrames.rend(), [&isLibrary](const PythonFrame &frame) {
        return !isLibrary(frame.path);
    });
    auto &frame = (it != pythonFrames.rend()) ? *it : pythonFrames.back();
    BuildMessage message;
    message.path = frame.path;
    message.line = frame.line;
    message.message = line;
    for (auto &each : pythonFrames) {
        message.detail.insert(message.detail.end(), each.lines.begin(), each.lines.end());
    }
    pythonFrames.clear();
    pendingContext.clear();
    Add(std::move(message), kContinuation::kNone, true);
    return true;
}

// Python's warnings module: '/work/main.py:5: DeprecationWarning: old api', the source line indented under it
bool BuildOutputParser::ParsePythonWarning(const std::string &line) {
    if (IsIndented(line)) {
        return false;
    }
    for (auto pos = line.find("Warning: "); pos != std::string::npos; pos = line.find("Warning: ", pos + 1)) {
        // ': <Category>Warning: ' - the category is one word
        auto start = line.rfind(": ", pos);
        if (start == std::string::npos) {
            continue;
        }
        auto category = std::string_view(line).substr(start + 2, pos - start - 2);
        if (!std::all_of(category.begin(), category.end(), [](char ch) { return std::isalnum((unsigned char)ch) || (ch == '_'); })) {
            continue;
        }
        BuildMessage message;
        SplitLocation(std::string_view(line).substr(0, start), message.path, message.line, message.column);
        if ((message.line == 0) || (message.column != 0) || message.path.empty()) {
            return false;
        }
        message.severity = BuildMessage::kSeverity::kWarning;
        message.message = line.substr(start + 2);
        Add(std::move(message), kContinuation::kCompiler, true);
        return true;
    }
    return false;
}

// rustc / cargo: 'error[E0308]: mismatched types' / 'warning: unused variable: `x`', the place on the ' --> ' line
// under it. Cargo's own summaries ('could not compile', 'generated 1 warning') are not messages.
bool BuildOutputParser::ParseRust(const std::string &line) {
    BuildMessage message;
    std::string_view rest;
    if (StartsWith(line, "error")) {
        rest = std::string_view(line).substr(5);
        message.severity = BuildMessage::kSeverity::kError;
    } else if (StartsWith(line, "warning")) {
        rest = std::string_view(line).substr(7);
        message.severity = BuildMessage::kSeverity::kWarning;
    } else {
        return false;
    }
    std::string code;
    if (StartsWith(rest, "[")) {
        auto close = rest.find(']');
        if (close == std::string_view::npos) {
            return false;
        }
        code = std::string(rest.substr(1, close - 1));
        rest = rest.substr(close + 1);
    }
    if (!StartsWith(rest, ": ")) {
        return false;
    }
    auto text = Trim(rest.substr(2));
    static const std::string_view summaries[] = {"could not compile ", "aborting due to ", "build failed"};
    for (auto summary : summaries) {
        if (StartsWith(text, summary)) {
            return true;
        }
    }
    if ((text.find(") generated ") != std::string::npos) && (text.find(" warning") != std::string::npos)) {
        return true;
    }
    message.message = code.empty() ? text : text + " [" + code + "]";
    Add(std::move(message), kContinuation::kRust, false);
    return true;
}

// GNU ld / gold: '/usr/bin/ld: main.cpp:(.text+0x9): undefined reference to `foo()'', with -g the line instead of
// the section ('/work/main.cpp:5:'), an object before the source ('b.o:/work/b.cpp:1: multiple definition of ...').
// An object file is no place to go to: the message is listed without one.
bool BuildOutputParser::ParseLinker(const std::string &line) {
    static const std::string_view markers[] = {": undefined reference to ", ": multiple definition of "};
    if (IsIndented(line)) {
        return false;
    }
    auto pos = std::string::npos;
    for (auto marker : markers) {
        pos = std::min(pos, line.find(marker));
    }
    if (pos == std::string::npos) {
        return false;
    }
    auto location = std::string_view(line).substr(0, pos);
    // '/usr/bin/ld: ' - the tool
    auto toolEnd = location.rfind(": ");
    if (toolEnd != std::string_view::npos) {
        location = location.substr(toolEnd + 2);
    }
    for (std::string_view object : {".o:", ".obj:"}) {
        auto objectEnd = location.find(object);
        if (objectEnd != std::string_view::npos) {
            location = location.substr(objectEnd + object.size());
            break;
        }
    }
    // ':(.text+0x9)' - the section
    auto section = location.find(":(");
    if (section != std::string_view::npos) {
        location = location.substr(0, section);
    }
    BuildMessage message;
    SplitLocation(location, message.path, message.line, message.column);
    auto extension = fs::path(message.path).extension().string();
    if ((extension == ".o") || (extension == ".obj") || (extension == ".a") || (extension == ".so") ||
        (message.path.find('(') != std::string::npos)) {
        message.path.clear();
        message.line = 0;
        message.column = 0;
    }
    message.message = line.substr(pos + 2);
    Add(std::move(message), kContinuation::kNone, true);
    return true;
}

// lld's '>>> referenced by main.cpp:7 (/work/main.cpp:7)' - the place of the 'undefined symbol' above, the absolute
// one in the parentheses when there is one
void BuildOutputParser::ParseLinkerReference(const std::string &line) {
    static const std::string_view referenced = ">>> referenced by ";
    if (isSkippingRepeat || messages.empty() || !messages.back().path.empty() || !StartsWith(line, referenced)) {
        return;
    }
    auto rest = std::string_view(line).substr(referenced.size());
    auto open = rest.find(" (");
    if ((open != std::string_view::npos) && (rest.back() == ')')) {
        rest = rest.substr(open + 2, rest.size() - open - 3);
    }
    auto &message = messages.back();
    SplitLocation(Trim(rest), message.path, message.line, message.column);
    if (message.line == 0) {
        message.path.clear();
    }
}

// 'CMake Error at CMakeLists.txt:12 (add_executable):', 'CMake Warning (dev) in x.cmake:', 'CMake Error: msg'
bool BuildOutputParser::ParseCMake(const std::string &line) {
    static const std::pair<std::string_view, BuildMessage::kSeverity> headers[] = {
        {"CMake Error", BuildMessage::kSeverity::kError},
        {"CMake Warning", BuildMessage::kSeverity::kWarning},
        {"CMake Deprecation Error", BuildMessage::kSeverity::kError},
        {"CMake Deprecation Warning", BuildMessage::kSeverity::kWarning},
    };
    std::string_view rest;
    BuildMessage message;
    bool isMatch = false;
    for (auto &[header, severity] : headers) {
        if (StartsWith(line, header)) {
            rest = std::string_view(line).substr(header.size());
            message.severity = severity;
            isMatch = true;
            break;
        }
    }
    if (!isMatch) {
        return false;
    }
    if (StartsWith(rest, " (dev)")) {
        rest.remove_prefix(6);
    }
    if (StartsWith(rest, ":")) {
        message.message = Trim(rest.substr(1));
        isCMakeAwaitingText = message.message.empty();
        Add(std::move(message), kContinuation::kCMake, false);
        return true;
    }
    if (!StartsWith(rest, " at ") && !StartsWith(rest, " in ")) {
        return false;
    }
    rest.remove_prefix(4);
    // 'path:line (command):' - the command is not part of the place
    auto end = rest.find(" (");
    if (end == std::string_view::npos) {
        end = rest.rfind(':');
    }
    if (end == std::string_view::npos) {
        return false;
    }
    SplitLocation(rest.substr(0, end), message.path, message.line, message.column);
    isCMakeAwaitingText = true;
    Add(std::move(message), kContinuation::kCMake, false);
    return true;
}

// gcc / clang: 'path:line:col: error: msg' ('warning:', 'fatal error:', 'note:'); a tool's own 'clang: error: ...'
bool BuildOutputParser::ParseCompiler(const std::string &line) {
    static const std::pair<std::string_view, BuildMessage::kSeverity> markers[] = {
        {": fatal error: ", BuildMessage::kSeverity::kError},
        {": error: ", BuildMessage::kSeverity::kError},
        {": warning: ", BuildMessage::kSeverity::kWarning},
        {": note: ", BuildMessage::kSeverity::kNote},
    };
    auto pos = std::string::npos;
    size_t markerSize = 0;
    BuildMessage message;
    // The line's trailing blanks are gone: 'x.cpp:3:4: warning:' when clang wrapped all of the message
    auto text = line + " ";
    for (auto &[marker, severity] : markers) {
        auto found = text.find(marker);
        if ((found != std::string::npos) && (found < pos)) {
            pos = found;
            markerSize = marker.size();
            message.severity = severity;
        }
    }
    if ((pos == std::string::npos) || (pos == 0) || IsIndented(line)) {
        return false;
    }
    auto location = std::string_view(line).substr(0, pos);
    message.message = Trim(std::string_view(text).substr(pos + markerSize));
    SplitLocation(location, message.path, message.line, message.column);
    if (message.line == 0) {
        // No place: only a tool's own line counts ('clang: error: linker command failed', 'ld: warning: ...'), so
        // a sentence that happens to hold ': error: ' doesn't
        if (location.find(' ') != std::string_view::npos) {
            return false;
        }
        message.message = std::string(location) + ": " + message.message;
        message.path.clear();
    }
    Add(std::move(message), kContinuation::kCompiler, true);
    isMessageWrapOpen = true;
    return true;
}

// 'In file included from a.h:3,' / '                 from b.cpp:1:' / 'x.cpp: In function 'int f()':' - kept for
// the message that follows
bool BuildOutputParser::ParseIncludeContext(const std::string &line) {
    if (StartsWith(line, "In file included from ")) {
        pendingContext.clear();
        pendingContext.push_back(line);
        return true;
    }
    if (!pendingContext.empty() && IsIndented(line) && StartsWith(Trim(line), "from ")) {
        pendingContext.push_back(line);
        return true;
    }
    // gcc's 'x.cpp: In function', ld's '/usr/bin/ld: main.o: in function `main':'
    if (!line.empty() && (line.back() == ':') && !IsIndented(line) &&
        ((line.find(": In ") != std::string::npos) || (line.find(": At global scope") != std::string::npos) ||
         (line.find(": in function `") != std::string::npos))) {
        pendingContext.push_back(line);
        return true;
    }
    return false;
}

void BuildOutputParser::Add(BuildMessage message, kContinuation nextContinuation, bool isRepeatChecked) {
    continuation = nextContinuation;
    message.directory = CurrentDirectory();
    message.outputLine = numLines;
    if (isRepeatChecked) {
        // The same header warning comes once per translation unit
        auto key = RepeatKey(message);
        if (!seen.insert(key).second) {
            isSkippingRepeat = true;
            pendingContext.clear();
            return;
        }
    }
    isSkippingRepeat = false;
    message.detail.insert(message.detail.begin(), pendingContext.begin(), pendingContext.end());
    pendingContext.clear();
    messages.push_back(std::move(message));
}

// The rest of a wrapped message: its repeat check is redone with the whole text
void BuildOutputParser::AppendWrapped(const std::string &text) {
    if (isSkippingRepeat || messages.empty()) {
        return;
    }
    auto &message = messages.back();
    seen.erase(RepeatKey(message));
    message.message += message.message.empty() ? text : " " + text;
    if (!seen.insert(RepeatKey(message)).second) {
        messages.pop_back();
        isSkippingRepeat = true;
    }
}

std::string BuildOutputParser::RepeatKey(const BuildMessage &message) {
    return std::to_string((int)message.severity) + "\x1f" + message.directory + "\x1f" + message.path + "\x1f" +
           std::to_string(message.line) + "\x1f" + std::to_string(message.column) + "\x1f" + message.message;
}

const std::string &BuildOutputParser::CurrentDirectory() const {
    static const std::string none;
    return directories.empty() ? none : directories.back();
}

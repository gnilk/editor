//
// The last build's errors and warnings - see BuildMessages.h, docs/build-messages.md
//

#include <algorithm>
#include <filesystem>
#include <sstream>

#include "BuildMessages.h"

using namespace gedit;
namespace fs = std::filesystem;

static std::vector<std::string> SplitWords(std::string_view text) {
    std::vector<std::string> words;
    std::istringstream stream{std::string(text)};
    std::string word;
    while (stream >> word) {
        words.push_back(word);
    }
    return words;
}

static bool HasPlace(const BuildMessage &message) {
    return !message.path.empty();
}

bool BuildMessages::IsBuildCommand(std::string_view cmdLine) {
    static const std::string_view tools[] = {"make", "gmake", "ninja", "cmake", "cc", "c++", "gcc", "g++", "clang", "clang++", "trun"};
    static const std::string_view versioned[] = {"gcc-", "g++-", "clang-", "clang++-"};
    static const std::string_view prefixes[] = {"time", "env", "nice", "command"};

    for (auto &word : SplitWords(cmdLine)) {
        // 'CC=clang make', 'time make -j8'
        if (word.find('=') != std::string::npos) {
            continue;
        }
        if (std::find(std::begin(prefixes), std::end(prefixes), word) != std::end(prefixes)) {
            continue;
        }
        auto name = fs::path(word).filename().string();
        if (std::find(std::begin(tools), std::end(tools), name) != std::end(tools)) {
            return true;
        }
        return std::any_of(std::begin(versioned), std::end(versioned), [&name](std::string_view prefix) {
            return (name.size() > prefix.size()) && (name.compare(0, prefix.size(), prefix) == 0);
        });
    }
    return false;
}

bool BuildMessages::OnCommand(const std::string &cmdLine, const std::string &shellDirectory, const std::string &root,
                              const void *terminal) {
    std::lock_guard<std::mutex> guard(lock);
    if (!IsBuildCommand(cmdLine)) {
        if (isRunning && (terminal == runTerminal)) {
            parser.Finish();
            isRunning = false;
        }
        return false;
    }
    parser.Reset();
    hasRun = true;
    isRunning = true;
    wasJobSeen = false;
    command = cmdLine;
    directory = shellDirectory;
    buildDirectory = BuildDirectoryOf(cmdLine);
    projectRoot = root;
    runTerminal = terminal;
    current.reset();
    return true;
}

void BuildMessages::Feed(std::string_view bytes, const void *terminal) {
    std::lock_guard<std::mutex> guard(lock);
    if (isRunning && (terminal == runTerminal)) {
        parser.Feed(bytes);
    }
}

bool BuildMessages::UpdateJobState(bool isJobRunning, const void *terminal) {
    {
        std::lock_guard<std::mutex> guard(lock);
        if (!isRunning || (terminal != runTerminal)) {
            return false;
        }
        if (isJobRunning) {
            wasJobSeen = true;
            return false;
        }
        if (!wasJobSeen) {
            return false;
        }
    }
    return EndRun();
}

bool BuildMessages::EndRun() {
    std::lock_guard<std::mutex> guard(lock);
    if (!isRunning) {
        return false;
    }
    parser.Finish();
    isRunning = false;
    return true;
}

bool BuildMessages::IsRunning() const {
    std::lock_guard<std::mutex> guard(lock);
    return isRunning;
}

bool BuildMessages::HasRun() const {
    std::lock_guard<std::mutex> guard(lock);
    return hasRun;
}

std::string BuildMessages::Command() const {
    std::lock_guard<std::mutex> guard(lock);
    return command;
}

std::vector<BuildMessage> BuildMessages::Messages() const {
    std::lock_guard<std::mutex> guard(lock);
    return parser.Messages();
}

size_t BuildMessages::NumErrors() const {
    std::lock_guard<std::mutex> guard(lock);
    return parser.NumErrors();
}

size_t BuildMessages::NumWarnings() const {
    std::lock_guard<std::mutex> guard(lock);
    return parser.NumWarnings();
}

std::string BuildMessages::Summary() const {
    std::lock_guard<std::mutex> guard(lock);
    auto nErrors = parser.NumErrors();
    auto nWarnings = parser.NumWarnings();
    if ((nErrors == 0) && (nWarnings == 0)) {
        return "no errors or warnings";
    }
    auto count = [](size_t n, const char *what) {
        return std::to_string(n) + " " + what + ((n == 1) ? "" : "s");
    };
    return count(nErrors, "error") + ", " + count(nWarnings, "warning");
}

std::optional<BuildMessages::Position> BuildMessages::Next() {
    return Step(1);
}

std::optional<BuildMessages::Position> BuildMessages::Previous() {
    return Step(-1);
}

std::optional<BuildMessages::Position> BuildMessages::Select(size_t index) {
    std::lock_guard<std::mutex> guard(lock);
    auto order = Order(parser.Messages());
    if (std::find(order.begin(), order.end(), index) == order.end()) {
        return std::nullopt;
    }
    current = index;
    return PositionOf(index);
}

std::string BuildMessages::ResolvePath(const BuildMessage &message) const {
    std::lock_guard<std::mutex> guard(lock);
    return ResolvePathLocked(message);
}

std::vector<size_t> BuildMessages::Order(const std::vector<BuildMessage> &messages) {
    std::vector<size_t> order;
    for (auto severity : {BuildMessage::kSeverity::kError, BuildMessage::kSeverity::kWarning}) {
        for (size_t i = 0; i < messages.size(); i++) {
            if ((messages[i].severity == severity) && HasPlace(messages[i])) {
                order.push_back(i);
            }
        }
    }
    return order;
}

std::optional<BuildMessages::Position> BuildMessages::Step(int direction) {
    std::lock_guard<std::mutex> guard(lock);
    auto order = Order(parser.Messages());
    if (order.empty()) {
        return std::nullopt;
    }
    auto n = order.size();
    // From before the first (forward) / after the last (backward) when nothing is current yet
    size_t at = (direction > 0) ? n - 1 : 0;
    if (current.has_value()) {
        auto it = std::find(order.begin(), order.end(), current.value());
        if (it != order.end()) {
            at = (size_t)(it - order.begin());
        }
    }
    at = (at + n + (size_t)(direction > 0 ? 1 : n - 1)) % n;
    current = order[at];
    return PositionOf(order[at]);
}

std::optional<BuildMessages::Position> BuildMessages::PositionOf(size_t index) const {
    auto &messages = parser.Messages();
    auto order = Order(messages);
    Position position;
    position.message = messages[index];
    position.resolvedPath = ResolvePathLocked(messages[index]);
    position.index = (size_t)(std::find(order.begin(), order.end(), index) - order.begin());
    position.count = order.size();
    return position;
}

// Absolute as it is; a relative one tried against make's / ninja's directory, the build directory the command
// named, the shell's directory, the project root - the first that has the file wins (§5)
std::string BuildMessages::ResolvePathLocked(const BuildMessage &message) const {
    if (message.path.empty()) {
        return {};
    }
    auto isFile = [](const fs::path &path) {
        std::error_code ec;
        return fs::is_regular_file(path, ec);
    };
    auto finish = [](const fs::path &path) {
        std::error_code ec;
        auto canonical = fs::weakly_canonical(path, ec);
        return ec ? path.lexically_normal().string() : canonical.string();
    };
    fs::path path(message.path);
    if (path.is_absolute()) {
        return isFile(path) ? finish(path) : std::string{};
    }
    auto underShell = [this](const std::string &dir) -> fs::path {
        fs::path candidate(dir);
        if (candidate.is_relative() && !directory.empty()) {
            return fs::path(directory) / candidate;
        }
        return candidate;
    };
    std::vector<fs::path> bases;
    if (!message.directory.empty()) {
        bases.push_back(underShell(message.directory));
    }
    if (!buildDirectory.empty()) {
        bases.push_back(underShell(buildDirectory));
    }
    if (!directory.empty()) {
        bases.push_back(directory);
    }
    if (!projectRoot.empty()) {
        bases.push_back(projectRoot);
    }
    for (auto &base : bases) {
        if (base.is_absolute() && isFile(base / path)) {
            return finish(base / path);
        }
    }
    return {};
}

std::string BuildMessages::BuildDirectoryOf(const std::string &cmdLine) {
    auto words = SplitWords(cmdLine);
    for (size_t i = 0; i + 1 < words.size(); i++) {
        if ((words[i] == "--build") || (words[i] == "-C")) {
            return words[i + 1];
        }
    }
    return {};
}

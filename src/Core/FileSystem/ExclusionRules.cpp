//
// Excludes + '.gitignore' - see ExclusionRules.h
//

#include <fnmatch.h>

#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>

#include "ExclusionRules.h"

using namespace gedit;
namespace fs = std::filesystem;

static std::vector<std::string> SplitOnSlash(const std::string &text) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, '/')) {
        parts.push_back(part);
    }
    return parts;
}

// Git strips trailing spaces unless they are escaped ('cmake-build-*/ ' is a folder pattern)
static void StripTrailingSpace(std::string &line) {
    while (!line.empty() && ((line.back() == '\r') || (line.back() == '\n'))) {
        line.pop_back();
    }
    while (!line.empty() && ((line.back() == ' ') || (line.back() == '\t'))) {
        bool isEscaped = (line.size() >= 2) && (line[line.size() - 2] == '\\');
        if (isEscaped) {
            break;
        }
        line.pop_back();
    }
}

bool ExclusionRules::ParseLine(const std::string &line, Pattern &outPattern) {
    std::string text = line;
    StripTrailingSpace(text);
    if (text.empty() || (text[0] == '#')) {
        return false;
    }
    outPattern = {};
    if (text[0] == '!') {
        outPattern.isNegated = true;
        text.erase(0, 1);
    } else if ((text.size() >= 2) && (text[0] == '\\') && ((text[1] == '!') || (text[1] == '#'))) {
        text.erase(0, 1);
    }
    if (!text.empty() && (text.back() == '/')) {
        outPattern.isDirectoryOnly = true;
        text.pop_back();
    }
    // A '/' left in it, at the start or in the middle, ties the pattern to this folder
    outPattern.isAnchored = text.find('/') != std::string::npos;
    if (!text.empty() && (text[0] == '/')) {
        text.erase(0, 1);
    }
    outPattern.segments = SplitOnSlash(text);
    return !outPattern.segments.empty();
}

static bool MatchSegments(const std::vector<std::string> &pattern, size_t idxPattern, const std::vector<std::string> &path, size_t idxPath) {
    if (idxPattern == pattern.size()) {
        return idxPath == path.size();
    }
    if (pattern[idxPattern] == "**") {
        // A trailing '/**' needs something below it; elsewhere it can span nothing
        if ((idxPattern + 1) == pattern.size()) {
            return idxPath < path.size();
        }
        for (auto idxNext = idxPath; idxNext <= path.size(); idxNext++) {
            if (MatchSegments(pattern, idxPattern + 1, path, idxNext)) {
                return true;
            }
        }
        return false;
    }
    if (idxPath == path.size()) {
        return false;
    }
    if (fnmatch(pattern[idxPattern].c_str(), path[idxPath].c_str(), 0) != 0) {
        return false;
    }
    return MatchSegments(pattern, idxPattern + 1, path, idxPath + 1);
}

bool ExclusionRules::Matches(const Pattern &pattern, const std::vector<std::string> &relPath, bool isDirectory) {
    if (relPath.empty() || (pattern.isDirectoryOnly && !isDirectory)) {
        return false;
    }
    if (pattern.isAnchored) {
        return MatchSegments(pattern.segments, 0, relPath, 0);
    }
    // No '/': the name, at any depth below the file
    return fnmatch(pattern.segments[0].c_str(), relPath.back().c_str(), 0) == 0;
}

fs::path ExclusionRules::DefaultGlobalIgnoreFile() {
    auto xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    if ((xdgConfigHome != nullptr) && (xdgConfigHome[0] != '\0')) {
        return fs::path(xdgConfigHome) / "git" / "ignore";
    }
    auto home = std::getenv("HOME");
    return (home != nullptr) ? (fs::path(home) / ".config" / "git" / "ignore") : fs::path{};
}

ExclusionRules::ExclusionRules(const fs::path &root, std::vector<std::string> excludePatterns, bool useGitIgnore, const fs::path &globalIgnoreFile) :
    root(root.lexically_normal()), excludeFilter(std::move(excludePatterns)), useGitIgnore(useGitIgnore) {
    if (useGitIgnore && !globalIgnoreFile.empty()) {
        globalPatterns = LoadPatterns(globalIgnoreFile);
    }
    std::error_code error;
    canonicalRoot = fs::canonical(root, error);
    if (error) {
        canonicalRoot = this->root;
    }
    repositoryTop = FindRepositoryTop();
}

// A folder opened inside a repository still follows the repository's rules
fs::path ExclusionRules::FindRepositoryTop() const {
    std::error_code error;
    for (auto folder = canonicalRoot; ; folder = folder.parent_path()) {
        if (fs::exists(folder / ".git", error)) {
            return folder;
        }
        if (folder == folder.parent_path()) {
            return canonicalRoot;
        }
    }
}

// False when 'path' isn't below 'base'
bool ExclusionRules::RelativeComponents(const fs::path &path, const fs::path &base, std::vector<std::string> &outComponents) const {
    auto relative = path.lexically_normal().lexically_relative(base);
    if (relative.empty() || (*relative.begin() == "..")) {
        return false;
    }
    outComponents.clear();
    for (const auto &component : relative) {
        auto name = component.string();
        if (!name.empty() && (name != ".")) {
            outComponents.push_back(std::move(name));
        }
    }
    return true;
}

// A root opened through a symlink hands out paths below either spelling
bool ExclusionRules::ComponentsBelowRoot(const fs::path &path, std::vector<std::string> &outComponents) const {
    return RelativeComponents(path, canonicalRoot, outComponents) || RelativeComponents(path, root, outComponents);
}

bool ExclusionRules::IsAnyExcluded(const std::vector<std::string> &components) const {
    for (auto &component : components) {
        if (excludeFilter.IsExcluded(component)) {
            return true;
        }
    }
    return false;
}

bool ExclusionRules::IsExcluded(const fs::path &path) const {
    std::vector<std::string> components;
    return ComponentsBelowRoot(path, components) && IsAnyExcluded(components);
}

ExclusionRules::kVerdict ExclusionRules::Check(const fs::path &path, bool isDirectory) const {
    std::vector<std::string> components;
    if (!ComponentsBelowRoot(path, components) || components.empty()) {
        return kVerdict::kIncluded;
    }
    if (IsAnyExcluded(components)) {
        return kVerdict::kExcluded;
    }
    if (!useGitIgnore) {
        return kVerdict::kIncluded;
    }
    // Rule files are relative to the repository, which can start above the root
    std::vector<std::string> fromTop;
    for (auto &component : SplitOnSlash(canonicalRoot.lexically_relative(repositoryTop).generic_string())) {
        if (!component.empty() && (component != ".")) {
            fromTop.push_back(component);
        }
    }
    fromTop.insert(fromTop.end(), components.begin(), components.end());
    return IsIgnored(repositoryTop, fromTop, isDirectory) ? kVerdict::kIgnored : kVerdict::kIncluded;
}

// Top down: each step is judged on its own, and an ignored folder ends it - what's inside can't come back
bool ExclusionRules::IsIgnored(const fs::path &top, const std::vector<std::string> &components, bool isDirectory) const {
    std::vector<fs::path> folders = {top};
    for (size_t numUsed = 1; numUsed <= components.size(); numUsed++) {
        bool isLast = numUsed == components.size();
        bool isStepDirectory = !isLast || isDirectory;

        bool isStepIgnored = false;
        bool isDecided = false;
        // Deepest file first, each one's last line first
        for (size_t idxFolder = folders.size(); (idxFolder > 0) && !isDecided; idxFolder--) {
            auto patterns = PatternsForFolder(folders[idxFolder - 1]);
            std::vector<std::string> relPath(components.begin() + (ptrdiff_t)(idxFolder - 1), components.begin() + (ptrdiff_t)numUsed);
            for (auto it = patterns->rbegin(); it != patterns->rend(); ++it) {
                if (Matches(*it, relPath, isStepDirectory)) {
                    isStepIgnored = !it->isNegated;
                    isDecided = true;
                    break;
                }
            }
        }
        // No file in the tree has a say: the user's own rules
        if (!isDecided) {
            std::vector<std::string> relPath(components.begin(), components.begin() + (ptrdiff_t)numUsed);
            for (auto it = globalPatterns.rbegin(); it != globalPatterns.rend(); ++it) {
                if (Matches(*it, relPath, isStepDirectory)) {
                    isStepIgnored = !it->isNegated;
                    break;
                }
            }
        }
        if (isStepIgnored) {
            return true;
        }
        if (!isLast) {
            folders.push_back(folders.back() / components[numUsed - 1]);
        }
    }
    return false;
}

std::shared_ptr<const ExclusionRules::Patterns> ExclusionRules::PatternsForFolder(const fs::path &folder) const {
    auto key = folder.string();
    {
        std::shared_lock guard(cacheLock);
        auto it = cache.find(key);
        if (it != cache.end()) {
            return it->second;
        }
    }
    auto patterns = std::make_shared<Patterns>(LoadPatterns(folder / ".gitignore"));
    std::unique_lock guard(cacheLock);
    cache[key] = patterns;
    return patterns;
}

ExclusionRules::Patterns ExclusionRules::LoadPatterns(const fs::path &path) {
    Patterns patterns;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        Pattern pattern;
        if (ParseLine(line, pattern)) {
            patterns.push_back(std::move(pattern));
        }
    }
    return patterns;
}

void ExclusionRules::Invalidate(const fs::path &folder) {
    std::unique_lock guard(cacheLock);
    cache.erase(folder.string());
}

void ExclusionRules::InvalidateAll() {
    std::unique_lock guard(cacheLock);
    cache.clear();
}

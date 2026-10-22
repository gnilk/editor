//
// See ConfigFilePatcher.h
//

#include <algorithm>
#include <optional>
#include <yaml-cpp/yaml.h>

#include "ConfigFilePatcher.h"

using namespace gedit;

namespace {
    // One line of the file, as far as block-structure goes
    struct LineInfo {
        bool isContent = false;     // false for blank and comment-only lines
        bool isSeqItem = false;     // '- ...'
        int indent = 0;
        std::string key;            // empty when the line isn't 'key:' (sequence item, continuation, ...)
        size_t valueStart = 0;      // index just past the ':' of 'key:'
    };

    // The value part of a 'key: value  # comment' line
    struct ValueParts {
        std::string leadingSpace;   // between ':' and the value
        std::string value;
        std::string commentGap;     // between the value and '#'
        std::string comment;        // from '#', empty if none
    };
}

static std::vector<std::string> SplitLines(const std::string &text, bool &outEndsWithNewline);
static std::string JoinLines(const std::vector<std::string> &lines, bool endsWithNewline,
                             size_t insertAt, const std::vector<std::string> &insertLines);
static LineInfo AnalyzeLine(const std::string &line);
static ValueParts SplitValue(const std::string &valueText);
static bool IsPlainKey(const std::string &key);
static bool IsPlainScalarChar(char ch);
static std::optional<std::string> Verify(const std::string &original, const std::string &patched,
                                         const std::vector<std::string> &keyPath, const std::string &value);

ConfigFilePatcher::Result ConfigFilePatcher::SetScalar(const std::string &text, const std::vector<std::string> &keyPath, const std::string &value) {
    Result result;
    if (keyPath.empty()) {
        result.error = "empty key";
        return result;
    }
    for (auto &segment : keyPath) {
        if (!IsPlainKey(segment)) {
            result.error = "unsupported key '" + segment + "' (letters, digits, '_' and '-' only)";
            return result;
        }
    }
    if (text.find('\r') != std::string::npos) {
        result.error = "the file has CRLF line endings";
        return result;
    }

    bool endsWithNewline = true;
    auto lines = SplitLines(text, endsWithNewline);
    std::vector<LineInfo> infos;
    infos.reserve(lines.size());
    for (auto &line : lines) {
        auto info = AnalyzeLine(line);
        if (info.isContent && (line.find_first_not_of(' ') < line.size()) && (line[line.find_first_not_of(' ')] == '\t')) {
            result.error = "the file uses tab indentation";
            return result;
        }
        infos.push_back(info);
    }

    auto formatted = FormatScalar(value);
    // Inserted lines are spliced in by JoinLines - 'lines' itself never grows
    size_t insertAt = lines.size();
    std::vector<std::string> insertLines;

    // Walk down the key path; [scopeBegin, scopeEnd) holds the lines below the current parent
    size_t scopeBegin = 0;
    size_t scopeEnd = lines.size();
    int parentIndent = -1;
    int parentLine = -1;
    for (size_t idxSegment = 0; idxSegment < keyPath.size(); idxSegment++) {
        // The children of the parent are the shallowest content lines in scope
        int childIndent = -1;
        int lastContentLine = -1;
        for (size_t i = scopeBegin; i < scopeEnd; i++) {
            if (!infos[i].isContent) {
                continue;
            }
            if ((childIndent < 0) || (infos[i].indent < childIndent)) {
                childIndent = infos[i].indent;
            }
            lastContentLine = static_cast<int>(i);
        }

        int found = -1;
        bool hasSeqItems = false;
        for (size_t i = scopeBegin; i < scopeEnd; i++) {
            if (!infos[i].isContent || (infos[i].indent != childIndent)) {
                continue;
            }
            if (infos[i].isSeqItem) {
                hasSeqItems = true;
            }
            if (infos[i].key == keyPath[idxSegment]) {
                found = static_cast<int>(i);
                break;
            }
        }

        if (found < 0) {
            if (hasSeqItems) {
                result.error = (idxSegment == 0) ? std::string("the file is a list") : "'" + keyPath[idxSegment - 1] + "' is a list";
                return result;
            }
            // Insert the missing key(s) after the last content line of this section - trailing comments
            // usually belong to whatever follows. A missing top-level section goes at the end of the file.
            int baseIndent = (childIndent >= 0) ? childIndent : ((parentIndent < 0) ? 0 : parentIndent + 2);
            if (parentLine < 0) {
                insertAt = lines.size();
            } else {
                insertAt = (lastContentLine >= 0) ? static_cast<size_t>(lastContentLine + 1) : static_cast<size_t>(parentLine + 1);
            }
            for (size_t j = idxSegment; j < keyPath.size(); j++) {
                std::string newLine(static_cast<size_t>(baseIndent) + 2 * (j - idxSegment), ' ');
                newLine += keyPath[j] + ":";
                if (j == keyPath.size() - 1) {
                    newLine += " " + formatted;
                }
                insertLines.push_back(newLine);
            }
            if ((insertAt == lines.size()) && !lines.empty() && !endsWithNewline) {
                endsWithNewline = true;     // the old last line now needs its newline
            }
            break;
        }

        auto &info = infos[found];
        auto parts = SplitValue(lines[found].substr(info.valueStart));
        // The block below this key: everything deeper, up to the next line at (or above) its indent
        size_t blockEnd = static_cast<size_t>(found) + 1;
        int firstChild = -1;
        while (blockEnd < scopeEnd) {
            if (infos[blockEnd].isContent) {
                bool isChild = (infos[blockEnd].indent > info.indent) || ((infos[blockEnd].indent == info.indent) && infos[blockEnd].isSeqItem);
                if (!isChild) {
                    break;
                }
                if (firstChild < 0) {
                    firstChild = static_cast<int>(blockEnd);
                }
            }
            blockEnd++;
        }

        bool isLast = (idxSegment == keyPath.size() - 1);
        if (isLast) {
            if (firstChild >= 0) {
                result.error = "'" + keyPath[idxSegment] + "' is a section or a list, not a single value";
                return result;
            }
            if (!parts.value.empty() && std::string("{[&*|>!").find(parts.value[0]) != std::string::npos) {
                result.error = "'" + keyPath[idxSegment] + "' is not a plain value";
                return result;
            }
            std::string newLine = lines[found].substr(0, info.valueStart);
            newLine += parts.leadingSpace.empty() ? " " : parts.leadingSpace;
            newLine += formatted;
            if (!parts.comment.empty()) {
                newLine += (parts.commentGap.empty() ? " " : parts.commentGap) + parts.comment;
            }
            lines[found] = newLine;
            break;
        }

        // An intermediate key must be a section (nothing but an optional comment after the ':')
        if (!parts.value.empty()) {
            result.error = "'" + keyPath[idxSegment] + "' is a value, not a section";
            return result;
        }
        scopeBegin = static_cast<size_t>(found) + 1;
        scopeEnd = blockEnd;
        parentIndent = info.indent;
        parentLine = found;
    }

    auto patched = JoinLines(lines, endsWithNewline, insertAt, insertLines);
    auto verifyError = Verify(text, patched, keyPath, value);
    if (verifyError.has_value()) {
        result.error = verifyError.value();
        return result;
    }
    result.isOk = true;
    result.text = patched;
    return result;
}

std::string ConfigFilePatcher::FormatScalar(const std::string &value) {
    bool isPlain = !value.empty() && std::all_of(value.begin(), value.end(), IsPlainScalarChar);
    if (isPlain) {
        return value;
    }
    std::string quoted = "\"";
    for (auto ch : value) {
        if ((ch == '"') || (ch == '\\')) {
            quoted += '\\';
        }
        quoted += ch;
    }
    quoted += "\"";
    return quoted;
}

//
// Helpers
//

// The patched text must parse to exactly "the original with this one value set" - same keys, same order,
// same values everywhere else. This is what makes the line-based edit safe without a fallback.
static std::optional<std::string> Verify(const std::string &original, const std::string &patched,
                                         const std::vector<std::string> &keyPath, const std::string &value) {
    YAML::Node expected;
    YAML::Node actual;
    try {
        expected = YAML::Load(original);
    } catch (const YAML::Exception &e) {
        return std::string("the file doesn't parse: ") + e.what();
    }
    if (expected.IsNull() || !expected.IsDefined()) {
        expected = YAML::Node(YAML::NodeType::Map);
    }
    if (!expected.IsMap()) {
        return std::string("the file is not a map");
    }
    YAML::Node node = expected;
    for (size_t i = 0; i < keyPath.size() - 1; i++) {
        node.reset(node[keyPath[i]]);   // reset() re-binds; operator= would assign INTO the node
    }
    node[keyPath.back()] = value;

    try {
        actual = YAML::Load(patched);
    } catch (const YAML::Exception &e) {
        return std::string("the edit doesn't parse: ") + e.what();
    }
    if (YAML::Dump(expected) != YAML::Dump(actual)) {
        return std::string("the edit can't be placed safely in this file");
    }
    return {};
}

static std::vector<std::string> SplitLines(const std::string &text, bool &outEndsWithNewline) {
    std::vector<std::string> lines;
    outEndsWithNewline = text.empty() || (text.back() == '\n');
    size_t start = 0;
    while (start < text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

static std::string JoinLines(const std::vector<std::string> &lines, bool endsWithNewline,
                             size_t insertAt, const std::vector<std::string> &insertLines) {
    std::string text;
    size_t nTotal = lines.size() + insertLines.size();
    size_t idxOut = 0;
    auto emit = [&](const std::string &line) {
        text += line;
        idxOut++;
        if ((idxOut < nTotal) || endsWithNewline) {
            text += "\n";
        }
    };
    for (size_t i = 0; i <= lines.size(); i++) {
        if (i == insertAt) {
            for (auto &line : insertLines) {
                emit(line);
            }
        }
        if (i < lines.size()) {
            emit(lines[i]);
        }
    }
    return text;
}

static LineInfo AnalyzeLine(const std::string &line) {
    LineInfo info;
    auto first = line.find_first_not_of(" \t");
    if ((first == std::string::npos) || (line[first] == '#')) {
        return info;
    }
    info.isContent = true;
    info.indent = static_cast<int>(first);
    if ((line[first] == '-') && ((first + 1 == line.size()) || (line[first + 1] == ' '))) {
        info.isSeqItem = true;
        return info;
    }
    // 'key:' followed by a space, a tab or the end of the line
    for (size_t i = first; i < line.size(); i++) {
        if (line[i] != ':') {
            continue;
        }
        if ((i + 1 == line.size()) || (line[i + 1] == ' ') || (line[i + 1] == '\t')) {
            auto key = line.substr(first, i - first);
            while (!key.empty() && (key.back() == ' ')) {
                key.pop_back();
            }
            if (IsPlainKey(key)) {
                info.key = key;
                info.valueStart = i + 1;
            }
            break;
        }
    }
    return info;
}

static ValueParts SplitValue(const std::string &valueText) {
    ValueParts parts;
    auto first = valueText.find_first_not_of(" \t");
    if (first == std::string::npos) {
        parts.leadingSpace = valueText;
        return parts;
    }
    parts.leadingSpace = valueText.substr(0, first);
    // Where the value ends: after a closing quote, or at the first '#' preceded by whitespace
    size_t valueEnd = valueText.size();
    size_t searchFrom = first;
    if ((valueText[first] == '"') || (valueText[first] == '\'')) {
        auto quote = valueText[first];
        size_t i = first + 1;
        while (i < valueText.size()) {
            if ((quote == '"') && (valueText[i] == '\\')) {
                i += 2;
                continue;
            }
            if (valueText[i] == quote) {
                if ((quote == '\'') && (i + 1 < valueText.size()) && (valueText[i + 1] == '\'')) {
                    i += 2;     // '' is an escaped quote in single-quoted scalars
                    continue;
                }
                break;
            }
            i++;
        }
        searchFrom = std::min(i + 1, valueText.size());
    }
    if (valueText[first] == '#') {
        valueEnd = first;
    } else {
        for (size_t i = searchFrom; i < valueText.size(); i++) {
            if ((valueText[i] == '#') && ((valueText[i - 1] == ' ') || (valueText[i - 1] == '\t'))) {
                valueEnd = i;
                break;
            }
        }
    }
    auto valueAndGap = valueText.substr(first, valueEnd - first);
    auto lastNonSpace = valueAndGap.find_last_not_of(" \t");
    if (lastNonSpace == std::string::npos) {
        parts.commentGap = valueAndGap;
    } else {
        parts.value = valueAndGap.substr(0, lastNonSpace + 1);
        parts.commentGap = valueAndGap.substr(lastNonSpace + 1);
    }
    parts.comment = valueText.substr(valueEnd);
    if (parts.value.empty()) {
        // Comment only - keep the original spacing in front of the comment as the comment's gap
        parts.commentGap = parts.leadingSpace + parts.commentGap;
        parts.leadingSpace.clear();
    }
    return parts;
}

static bool IsPlainKey(const std::string &key) {
    if (key.empty()) {
        return false;
    }
    return std::all_of(key.begin(), key.end(), [](char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) || (ch == '_') || (ch == '-');
    });
}

static bool IsPlainScalarChar(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || (std::string("_./~+-").find(ch) != std::string::npos);
}

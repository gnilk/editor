//
// See AgentQuestionTemplate.h
//

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

#include <fmt/format.h>
#include <yaml-cpp/yaml.h>

#include "AgentQuestionTemplate.h"

using namespace gedit;
namespace fs = std::filesystem;

// What 'needs' may name - the editor fills them from the active document
static const std::set<std::string> KNOWN_NEEDS = {"file", "line", "symbol"};
// Always there
static const std::set<std::string> ALWAYS_FILLED = {"root", "goatedit"};

static bool IsNameChar(char ch) {
    return ((ch >= 'a') && (ch <= 'z')) || ((ch >= '0') && (ch <= '9')) || (ch == '_');
}

// A placeholder at 'pos' ('{' there): its name and length with the braces, or nothing when the braces hold something
// else ('{ return x; }' in a code block stays as it is)
static std::optional<std::pair<std::string, size_t>> PlaceholderAt(const std::string &text, size_t pos) {
    size_t idx = pos + 1;
    bool hasColon = false;
    while ((idx < text.size()) && (IsNameChar(text[idx]) || ((text[idx] == ':') && !hasColon && (idx > pos + 1)))) {
        hasColon = hasColon || (text[idx] == ':');
        idx++;
    }
    if ((idx == pos + 1) || (idx >= text.size()) || (text[idx] != '}') || (text[idx - 1] == ':')) {
        return std::nullopt;
    }
    return std::make_pair(text.substr(pos + 1, idx - pos - 1), idx - pos + 1);
}

static std::vector<std::string> StringList(const YAML::Node &node) {
    std::vector<std::string> list;
    if (node.IsSequence()) {
        for (auto item : node) {
            list.push_back(item.as<std::string>());
        }
    } else if (node.IsScalar()) {
        list.push_back(node.as<std::string>());
    }
    return list;
}

static std::vector<std::string> SplitOnSpaces(const std::string &text) {
    std::vector<std::string> words;
    std::istringstream in(text);
    std::string word;
    while (in >> word) {
        words.push_back(word);
    }
    return words;
}

AgentQuestionTemplate::Ref AgentQuestionTemplate::FromText(const std::string &text, std::string &outError) {
    // The front matter: '---' on the first line, up to the next '---' line
    if (text.rfind("---\n", 0) != 0) {
        outError = "no front matter (a '---' line first)";
        return nullptr;
    }
    auto end = text.find("\n---", 3);
    if (end == std::string::npos) {
        outError = "the front matter has no end ('---')";
        return nullptr;
    }
    auto bodyStart = text.find('\n', end + 4);
    auto frontMatter = text.substr(4, end - 3);

    auto result = std::make_shared<AgentQuestionTemplate>();
    try {
        auto node = YAML::Load(frontMatter);
        if (!node.IsMap()) {
            outError = "the front matter is not a map";
            return nullptr;
        }
        result->name = node["name"] ? node["name"].as<std::string>() : "";
        result->title = node["title"] ? node["title"].as<std::string>() : "";
        result->needs = StringList(node["needs"]);
        result->args = StringList(node["args"]);
        result->usage = node["usage"] ? node["usage"].as<std::string>() : "";
        if (node["max_lines"]) {
            auto lines = node["max_lines"].as<int>();
            result->maxLines = (lines > 0) ? (size_t)lines : DEFAULT_MAX_LINES;
        }
        if (auto tools = node["tools"]; tools) {
            if (!tools.IsMap()) {
                outError = "'tools' is not a map of key: command";
                return nullptr;
            }
            for (auto item : tools) {
                Tool tool;
                tool.key = item.first.as<std::string>();
                if (tool.key.empty() || !std::all_of(tool.key.begin(), tool.key.end(), IsNameChar)) {
                    outError = "the tool key '" + tool.key + "' - lower case letters, digits and '_'";
                    return nullptr;
                }
                tool.command = SplitOnSpaces(item.second.as<std::string>());
                if (tool.command.empty()) {
                    outError = "the tool '" + tool.key + "' has no command";
                    return nullptr;
                }
                result->tools.push_back(std::move(tool));
            }
        }
    } catch (const YAML::Exception &ex) {
        outError = std::string("the front matter: ") + ex.what();
        return nullptr;
    }
    if (result->name.empty() || !std::all_of(result->name.begin(), result->name.end(), IsNameChar)) {
        outError = "'name' is missing, or not lower case letters, digits and '_'";
        return nullptr;
    }
    for (auto &need : result->needs) {
        if (KNOWN_NEEDS.count(need) == 0) {
            outError = "'needs' names '" + need + "' - it can be file, line, symbol";
            return nullptr;
        }
    }
    result->prompt = (bodyStart == std::string::npos) ? "" : text.substr(bodyStart + 1);
    // Leading blank lines aren't part of the prompt
    result->prompt.erase(0, result->prompt.find_first_not_of('\n'));
    if (result->prompt.find_first_not_of(" \t\n") == std::string::npos) {
        outError = "no prompt after the front matter";
        return nullptr;
    }
    if (!result->CheckPlaceholders(result->prompt, outError)) {
        return nullptr;
    }
    for (auto &tool : result->tools) {
        for (size_t i = 1; i < tool.command.size(); i++) {
            if (!result->CheckPlaceholders(tool.command[i], outError)) {
                outError = "the tool '" + tool.key + "': " + outError;
                return nullptr;
            }
        }
    }
    return result;
}

AgentQuestionTemplate::Ref AgentQuestionTemplate::FromFile(const fs::path &path, std::string &outError) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        outError = "can't read it";
        return nullptr;
    }
    std::stringstream text;
    text << in.rdbuf();
    auto result = FromText(text.str(), outError);
    if (result != nullptr) {
        result->origin = path;
    }
    return result;
}

std::vector<AgentQuestionTemplate::Ref> AgentQuestionTemplate::LoadAll(const std::vector<fs::path> &folders, std::vector<std::string> &outErrors) {
    std::map<std::string, Ref> byName;
    for (auto &folder : folders) {
        std::error_code ec;
        std::vector<fs::path> files;
        for (auto &entry : fs::directory_iterator(folder, ec)) {
            if (entry.is_regular_file() && (entry.path().extension() == ".md")) {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());
        for (auto &file : files) {
            std::string error;
            auto loaded = FromFile(file, error);
            if (loaded == nullptr) {
                outErrors.push_back(file.string() + ": " + error);
                continue;
            }
            byName[loaded->Name()] = loaded;
        }
    }
    std::vector<Ref> templates;
    for (auto &[templateName, loaded] : byName) {
        templates.push_back(loaded);
    }
    return templates;
}

AgentQuestionTemplate::Ref AgentQuestionTemplate::Find(const std::vector<Ref> &templates, const std::string &name) {
    for (auto &candidate : templates) {
        if (candidate->Name() == name) {
            return candidate;
        }
    }
    return nullptr;
}

fs::path AgentQuestionTemplate::FindTool(const std::vector<fs::path> &folders, const std::string &name) {
    // A name, not a path - a template can't reach outside the tool folders ('\\' is a separator on Windows)
    if (name.empty() || (name.find_first_of("/\\") != std::string::npos) || (name == "..") || (name == ".")) {
        return {};
    }
    for (auto it = folders.rbegin(); it != folders.rend(); ++it) {
        std::error_code ec;
        auto candidate = *it / name;
        if (fs::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

AgentQuestionTemplate::Command AgentQuestionTemplate::ToolCommand(const fs::path &toolFile, const std::vector<std::string> &arguments) {
    Command command;
    if (toolFile.extension() == ".py") {
        command.program = "python3";
        command.arguments.push_back(toolFile.string());
    } else {
        command.program = toolFile.string();
    }
    command.arguments.insert(command.arguments.end(), arguments.begin(), arguments.end());
    return command;
}

std::string AgentQuestionTemplate::CapOutput(const std::string &output, size_t maxLines) {
    // Where line 'maxLines + 1' starts
    size_t pos = 0;
    for (size_t i = 0; (i < maxLines) && (pos < output.size()); i++) {
        auto end = output.find('\n', pos);
        pos = (end == std::string::npos) ? output.size() : end + 1;
    }
    if (pos >= output.size()) {
        return output;
    }
    auto rest = output.substr(pos);
    auto numCut = std::count(rest.begin(), rest.end(), '\n') + ((rest.back() != '\n') ? 1 : 0);
    return output.substr(0, pos) + fmt::format("[... {} more lines cut - the output is kept to {} lines]\n", numCut, maxLines);
}

std::string AgentQuestionTemplate::Usage() const {
    if (!usage.empty()) {
        return usage;
    }
    std::string text;
    for (auto &arg : args) {
        text += (text.empty() ? "" : " ") + ("[" + arg + "]");
    }
    return text;
}

bool AgentQuestionTemplate::Needs(const std::string &what) const {
    return std::find(needs.begin(), needs.end(), what) != needs.end();
}

bool AgentQuestionTemplate::ArgValues(const std::vector<std::string> &words, Values &inOutValues) const {
    if (words.size() > args.size()) {
        return false;
    }
    for (size_t i = 0; i < args.size(); i++) {
        inOutValues[args[i]] = (i < words.size()) ? words[i] : "";
    }
    return true;
}

std::vector<std::string> AgentQuestionTemplate::ToolArguments(const Tool &tool, const Values &values) const {
    std::vector<std::string> arguments;
    for (size_t i = 1; i < tool.command.size(); i++) {
        auto filled = Fill(tool.command[i], values);
        if (!filled.empty()) {
            arguments.push_back(filled);
        }
    }
    return arguments;
}

std::string AgentQuestionTemplate::FillPrompt(const Values &values) const {
    return Fill(prompt, values);
}

std::string AgentQuestionTemplate::Describe(const Values &values) const {
    std::string where;
    if (Needs("file")) {
        where = values.count("file") ? values.at("file") : "";
        if (Needs("line") && values.count("line")) {
            where += ":" + values.at("line");
        }
    }
    if (Needs("symbol") && values.count("symbol")) {
        where += (where.empty() ? "" : " ") + values.at("symbol");
    }
    for (auto &arg : args) {
        if (values.count(arg) && !values.at(arg).empty()) {
            where += (where.empty() ? "" : " ") + arg + " " + values.at(arg);
        }
    }
    auto text = title.empty() ? name : title;
    return where.empty() ? text : text + " (" + where + ")";
}

bool AgentQuestionTemplate::CheckPlaceholders(const std::string &text, std::string &outError) const {
    for (size_t pos = text.find('{'); pos != std::string::npos; pos = text.find('{', pos + 1)) {
        if ((pos + 1 < text.size()) && (text[pos + 1] == '{')) {
            pos++;
            continue;
        }
        auto placeholder = PlaceholderAt(text, pos);
        if (!placeholder.has_value()) {
            continue;
        }
        auto &placeholderName = placeholder->first;
        bool isKnown = Needs(placeholderName) || (ALWAYS_FILLED.count(placeholderName) > 0) ||
                       (std::find(args.begin(), args.end(), placeholderName) != args.end());
        if (placeholderName.rfind("tool:", 0) == 0) {
            auto key = placeholderName.substr(5);
            isKnown = std::any_of(tools.begin(), tools.end(), [&key](const Tool &tool) { return tool.key == key; });
        }
        if (!isKnown) {
            outError = "unknown placeholder {" + placeholderName + "} - not in needs, args or tools";
            return false;
        }
    }
    return true;
}

std::string AgentQuestionTemplate::Fill(const std::string &text, const Values &values) {
    std::string result;
    size_t pos = 0;
    while (pos < text.size()) {
        auto brace = text.find('{', pos);
        if (brace == std::string::npos) {
            result += text.substr(pos);
            break;
        }
        result += text.substr(pos, brace - pos);
        if ((brace + 1 < text.size()) && (text[brace + 1] == '{')) {
            result += '{';
            pos = brace + 2;
            continue;
        }
        auto placeholder = PlaceholderAt(text, brace);
        if (!placeholder.has_value()) {
            result += '{';
            pos = brace + 1;
            continue;
        }
        auto it = values.find(placeholder->first);
        result += (it != values.end()) ? it->second : "";
        pos = brace + placeholder->second;
    }
    return result;
}

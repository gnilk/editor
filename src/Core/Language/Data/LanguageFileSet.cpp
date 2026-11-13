//
// Language files and their 'inherit' chains - see the header for the merge rules.
//

#include <algorithm>
#include <fstream>
#include <sstream>

#include "LanguageFileSet.h"

using namespace gedit;

static const std::string APPEND_SUFFIX = "+";

bool LanguageFileSet::AddFromString(const std::string &yamlText, const std::string &origin) {
    YAML::Node root;
    try {
        root = YAML::Load(yamlText);
    } catch (const YAML::Exception &e) {
        AddProblem(origin, e.mark.line + 1, e.msg);
        return false;
    }
    if (!root.IsMap() || !root["language"].IsMap()) {
        AddProblem(origin, 0, "no 'language:' section");
        return false;
    }
    auto nameNode = root["language"]["name"];
    if (!nameNode.IsScalar() || nameNode.Scalar().empty()) {
        AddProblem(origin, root["language"].Mark().line + 1, "'language:' has no 'name'");
        return false;
    }
    auto name = nameNode.Scalar();
    // erase + emplace: assigning over the old entry would write into the old file's nodes (YAML::Node's operator=)
    files.erase(name);
    files.emplace(name, File{origin, root});
    // A new file can change what anything inheriting from it resolves to
    resolved.clear();
    return true;
}

size_t LanguageFileSet::AddFromDirectory(const std::filesystem::path &directory) {
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) {
        return 0;
    }
    std::vector<std::filesystem::path> paths;
    for (auto &entry : std::filesystem::directory_iterator(directory, ec)) {
        if (entry.is_regular_file(ec) && (entry.path().extension() == ".yml")) {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());

    size_t nAdded = 0;
    for (auto &path : paths) {
        std::ifstream stream(path);
        if (!stream.is_open()) {
            AddProblem(path.string(), 0, "can't be read");
            continue;
        }
        std::stringstream text;
        text << stream.rdbuf();
        if (AddFromString(text.str(), path.string())) {
            nAdded++;
        }
    }
    return nAdded;
}

void LanguageFileSet::SetDefaultOverrides(const YAML::Node &newDefaultOverrides) {
    defaultOverrides.reset(newDefaultOverrides);
    resolved.clear();
}

YAML::Node LanguageFileSet::Resolve(const std::string &name) {
    auto it = resolved.find(name);
    if (it != resolved.end()) {
        return it->second;
    }
    std::set<std::string> inProgress;
    auto node = ResolveChain(name, inProgress);
    resolved.erase(name);
    resolved.emplace(name, node);
    return node;
}

std::vector<std::string> LanguageFileSet::Names() const {
    std::vector<std::string> names;
    for (auto &[name, file] : files) {
        names.push_back(name);
    }
    return names;
}

bool LanguageFileSet::Has(const std::string &name) const {
    return files.find(name) != files.end();
}

std::string LanguageFileSet::OriginOf(const std::string &name) const {
    auto it = files.find(name);
    if (it == files.end()) {
        return {};
    }
    return it->second.origin;
}

YAML::Node LanguageFileSet::ResolveChain(const std::string &name, std::set<std::string> &inProgress) {
    auto itFile = files.find(name);
    if (itFile == files.end()) {
        return YAML::Node(YAML::NodeType::Undefined);
    }
    auto &file = itFile->second;
    if (inProgress.find(name) != inProgress.end()) {
        AddProblem(file.origin, 0, "'inherit' goes round in a circle through '" + name + "'");
        return YAML::Node(YAML::NodeType::Undefined);
    }
    inProgress.insert(name);

    YAML::Node base(YAML::NodeType::Map);
    auto inheritNode = file.root["language"]["inherit"];
    if (inheritNode.IsDefined()) {
        auto parentName = inheritNode.as<std::string>("");
        if (!Has(parentName)) {
            AddProblem(file.origin, inheritNode.Mark().line + 1, "inherits '" + parentName + "', which there is no file for");
            inProgress.erase(name);
            return YAML::Node(YAML::NodeType::Undefined);
        }
        // A parent already resolved by an earlier language is reused; a fresh one is kept for the next
        auto itResolved = resolved.find(parentName);
        if (itResolved != resolved.end()) {
            base.reset(itResolved->second);
        } else {
            base.reset(ResolveChain(parentName, inProgress));
            resolved.erase(parentName);
            resolved.emplace(parentName, base);
        }
        if (!base.IsDefined()) {
            inProgress.erase(name);
            return base;
        }
    }

    auto result = Merge(base, file.root);
    if ((name == DEFAULT_NAME) && defaultOverrides.IsMap()) {
        YAML::Node overrides(YAML::NodeType::Map);
        overrides["language"] = defaultOverrides;
        result.reset(Merge(result, overrides));
    }
    inProgress.erase(name);
    // The chain is folded in - the result is no longer a child of anything
    return WithoutInherit(result);
}

//
// A new tree: map nodes are new, the leaves and lists are the inputs' own nodes (their marks intact).
// Each key is written once, so no node of either input is ever assigned into.
//
YAML::Node LanguageFileSet::Merge(const YAML::Node &base, const YAML::Node &child) {
    if (!child.IsDefined() || child.IsNull()) {
        return base;
    }
    if (!base.IsMap() || !child.IsMap()) {
        return child;
    }

    std::vector<std::string> keys;
    std::map<std::string, YAML::Node> values;
    // Never 'values[key] = value': YAML::Node's operator= writes through to the node it shares - an input's
    // own node. Copy-constructing only shares.
    auto setValue = [&keys, &values](const std::string &key, const YAML::Node &value) {
        if (values.find(key) == values.end()) {
            keys.push_back(key);
        }
        values.erase(key);
        values.emplace(key, value);
    };

    for (auto it : base) {
        setValue(it.first.Scalar(), it.second);
    }
    // Plain keys first, so 'key' and 'key+' in the same map mean: replace, then add
    for (auto it : child) {
        auto key = it.first.Scalar();
        if (key.ends_with(APPEND_SUFFIX) && (key.size() > APPEND_SUFFIX.size())) {
            continue;
        }
        auto itBase = values.find(key);
        if ((itBase != values.end()) && itBase->second.IsMap() && it.second.IsMap()) {
            setValue(key, Merge(itBase->second, it.second));
        } else {
            setValue(key, it.second);
        }
    }
    for (auto it : child) {
        auto key = it.first.Scalar();
        if (!key.ends_with(APPEND_SUFFIX) || (key.size() <= APPEND_SUFFIX.size())) {
            continue;
        }
        auto listKey = key.substr(0, key.size() - APPEND_SUFFIX.size());
        auto itBase = values.find(listKey);
        if ((itBase == values.end()) || !itBase->second.IsSequence() || !it.second.IsSequence()) {
            // Nothing to add to: the list is just the child's
            setValue(listKey, it.second);
            continue;
        }
        YAML::Node joined(YAML::NodeType::Sequence);
        for (auto element : itBase->second) {
            joined.push_back(element);
        }
        for (auto element : it.second) {
            joined.push_back(element);
        }
        setValue(listKey, joined);
    }

    YAML::Node result(YAML::NodeType::Map);
    for (auto &key : keys) {
        // A fresh key of a fresh map: the assignment rebinds that new node, nothing else
        result[key] = values.at(key);
    }
    return result;
}

// 'language:' without 'inherit', as a new tree - the section may still be the file's own node, so it is never
// changed in place
YAML::Node LanguageFileSet::WithoutInherit(const YAML::Node &root) {
    if (!root["language"].IsMap() || !root["language"]["inherit"].IsDefined()) {
        return root;
    }
    YAML::Node language(YAML::NodeType::Map);
    for (auto it : root["language"]) {
        if (it.first.Scalar() != "inherit") {
            language[it.first.Scalar()] = it.second;
        }
    }
    YAML::Node result(YAML::NodeType::Map);
    for (auto it : root) {
        if (it.first.Scalar() == "language") {
            result["language"] = language;
        } else {
            result[it.first.Scalar()] = it.second;
        }
    }
    return result;
}

void LanguageFileSet::AddProblem(const std::string &origin, int line, const std::string &reason) {
    problems.push_back({origin, line, reason});
}

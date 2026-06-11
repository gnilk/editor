//
// Created by gnilk on 11.06.26.
//
#include "AutoPairCache.h"

#include "Core/Config/Config.h"
#include "Core/RuntimeConfig.h"
#include "Core/UnicodeHelper.h"
#include "logger.h"

using namespace gedit;

// First Unicode codepoint of a (UTF-8) config string - the pair open/close chars are single glyphs.
static char32_t FirstChar32(const std::string &utf8) {
    if (utf8.empty()) {
        return 0;
    }
    auto u32 = UnicodeHelper::utf8to32(utf8);
    return u32.empty() ? 0 : u32[0];
}

// Map a 'suppress_in' name to the concrete token classes it covers.
static void AddSuppressClasses(const std::string &name, std::set<kLanguageTokenClass> &out) {
    if (name == "string") {
        out.insert(kLanguageTokenClass::kString);
        out.insert(kLanguageTokenClass::kChar);
    } else if (name == "comment") {
        out.insert(kLanguageTokenClass::kLineComment);
        out.insert(kLanguageTokenClass::kBlockComment);
        out.insert(kLanguageTokenClass::kCommentedText);
    }
}

AutoPairCache &AutoPairCache::Instance() {
    static AutoPairCache instance;
    return instance;
}

AutoPairTable::Ref AutoPairCache::GetTableForLanguage(const std::string &languageName) {
    auto it = cache.find(languageName);
    if (it != cache.end()) {
        return it->second;
    }

    AutoPairTable::Ref table;
    if (EnsureLoaded()) {
        std::set<std::string> inProgress;
        table = BuildTable(languageName, inProgress);
    }
    if (table == nullptr) {
        // No section / no config: a shared empty table => the engine is a no-op for this language.
        table = std::make_shared<AutoPairTable>();
    }
    cache[languageName] = table;
    return table;
}

void AutoPairCache::Clear() {
    cache.clear();
    autopairsRoot.reset();
    didAttemptLoad = false;
}

bool AutoPairCache::LoadFromString(const std::string &yaml) {
    Clear();
    didAttemptLoad = true;
    auto cfg = ConfigNode::FromString(yaml);
    if (!cfg.has_value()) {
        return false;
    }
    autopairsRoot = cfg->GetNode("autopairs");
    return autopairsRoot.has_value();
}

bool AutoPairCache::EnsureLoaded() {
    if (didAttemptLoad) {
        return autopairsRoot.has_value();
    }
    didAttemptLoad = true;

    auto logger = gnilk::Logger::GetLogger("AutoPairCache");
    auto fileName = Config::Instance().GetNode("main").GetStr("autopairs", "autopairs.yml");
    auto assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto asset = assetLoader.LoadTextAsset(fileName);
    if (asset == nullptr) {
        logger->Warning("autopairs config '%s' not found - auto-pairing disabled", fileName.c_str());
        return false;
    }
    auto cfg = ConfigNode::FromString(asset->GetPtrAs<const char *>());
    if (!cfg.has_value()) {
        logger->Error("autopairs config '%s' could not be parsed", fileName.c_str());
        return false;
    }
    autopairsRoot = cfg->GetNode("autopairs");
    return autopairsRoot.has_value();
}

// Resolve a language section into a table: fold the inherited parent first (so the child overrides/appends),
// then apply this section's own rules. Cycle-guarded via inProgress.
AutoPairTable::Ref AutoPairCache::BuildTable(const std::string &languageName, std::set<std::string> &inProgress) {
    if (!autopairsRoot.has_value() || !autopairsRoot->HasKey(languageName)) {
        return nullptr;
    }
    if (inProgress.find(languageName) != inProgress.end()) {
        return nullptr;   // inheritance cycle
    }
    inProgress.insert(languageName);

    auto section = autopairsRoot->GetNode(languageName);
    auto table = std::make_shared<AutoPairTable>();

    auto parentName = section.GetStr("inherit", "");
    if (!parentName.empty()) {
        auto parent = BuildTable(parentName, inProgress);
        if (parent != nullptr) {
            *table = *parent;   // start from the parent's fully-resolved rules
        }
    }
    ApplySection(section, *table);

    inProgress.erase(languageName);
    return table;
}

// Merge one section's declared rules onto the table. suppress_in / auto_close_before REPLACE the inherited
// set when the section declares them; pairs are appended (a child redefining an opener replaces that pair).
void AutoPairCache::ApplySection(const ConfigNode &section, AutoPairTable &table) {
    if (section.HasKey("suppress_in")) {
        table.suppressIn.clear();
        for (auto &name : section.GetSequenceOfStr("suppress_in")) {
            AddSuppressClasses(name, table.suppressIn);
        }
    }

    if (section.HasKey("auto_close_before")) {
        table.autoCloseBefore.clear();
        table.autoCloseBeforeEol = false;
        for (auto &tok : section.GetSequenceOfStr("auto_close_before")) {
            if (tok == "eol") {
                table.autoCloseBeforeEol = true;
            } else if (tok == "whitespace") {
                table.autoCloseBefore.insert(U' ');
                table.autoCloseBefore.insert(U'\t');
            } else if (!tok.empty()) {
                table.autoCloseBefore.insert(FirstChar32(tok));
            }
        }
    }

    // pairs: a YAML sequence of maps - iterate the raw node (ConfigNode only maps scalar sequences).
    const YAML::Node &sec = section.GetDataNode();
    if (sec["pairs"] && sec["pairs"].IsSequence()) {
        for (auto it = sec["pairs"].begin(); it != sec["pairs"].end(); ++it) {
            const YAML::Node &p = *it;
            AutoPairRule rule;
            rule.open = FirstChar32(p["open"].as<std::string>(""));
            rule.close = FirstChar32(p["close"].as<std::string>(""));
            rule.isQuote = p["quote"] ? p["quote"].as<bool>() : false;
            if ((rule.open == 0) || (rule.close == 0)) {
                continue;
            }
            // Replace an inherited pair with the same opener, else append.
            bool replaced = false;
            for (auto &existing : table.pairs) {
                if (existing.open == rule.open) {
                    existing = rule;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                table.pairs.push_back(rule);
            }
        }
    }
}

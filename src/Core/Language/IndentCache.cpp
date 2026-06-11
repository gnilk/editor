//
// Created by gnilk on 11.06.26.
//
#include "IndentCache.h"

#include "Core/Config/Config.h"
#include "Core/RuntimeConfig.h"
#include "Core/UnicodeHelper.h"
#include "logger.h"

using namespace gedit;

// First Unicode codepoint of a (UTF-8) config string - the trigger chars are single glyphs.
static char32_t FirstChar32(const std::string &utf8) {
    if (utf8.empty()) {
        return 0;
    }
    auto u32 = UnicodeHelper::utf8to32(utf8);
    return u32.empty() ? 0 : u32[0];
}

// Map a 'suppress_in' name to the concrete token classes it covers (same mapping as AutoPairCache).
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

IndentCache &IndentCache::Instance() {
    static IndentCache instance;
    return instance;
}

IndentTable::Ref IndentCache::GetTableForLanguage(const std::string &languageName) {
    auto it = cache.find(languageName);
    if (it != cache.end()) {
        return it->second;
    }

    IndentTable::Ref table;
    if (EnsureLoaded()) {
        std::set<std::string> inProgress;
        table = BuildTable(languageName, inProgress);
    }
    if (table == nullptr) {
        // No section / no config: a shared empty table => copy-previous-indent, no electric dedent.
        table = std::make_shared<IndentTable>();
    }
    cache[languageName] = table;
    return table;
}

void IndentCache::Clear() {
    cache.clear();
    indentRoot.reset();
    didAttemptLoad = false;
}

bool IndentCache::LoadFromString(const std::string &yaml) {
    Clear();
    didAttemptLoad = true;
    auto cfg = ConfigNode::FromString(yaml);
    if (!cfg.has_value()) {
        return false;
    }
    indentRoot = cfg->GetNode("indent");
    return indentRoot.has_value();
}

bool IndentCache::EnsureLoaded() {
    if (didAttemptLoad) {
        return indentRoot.has_value();
    }
    didAttemptLoad = true;

    auto logger = gnilk::Logger::GetLogger("IndentCache");
    auto fileName = Config::Instance().GetNode("main").GetStr("indent", "indent.yml");
    auto assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto asset = assetLoader.LoadTextAsset(fileName);
    if (asset == nullptr) {
        logger->Warning("indent config '%s' not found - auto-indent rules disabled", fileName.c_str());
        return false;
    }
    auto cfg = ConfigNode::FromString(asset->GetPtrAs<const char *>());
    if (!cfg.has_value()) {
        logger->Error("indent config '%s' could not be parsed", fileName.c_str());
        return false;
    }
    indentRoot = cfg->GetNode("indent");
    return indentRoot.has_value();
}

// Resolve a language section into a table: fold the inherited parent first (so the child overrides), then
// apply this section's own rules. Cycle-guarded via inProgress.
IndentTable::Ref IndentCache::BuildTable(const std::string &languageName, std::set<std::string> &inProgress) {
    if (!indentRoot.has_value() || !indentRoot->HasKey(languageName)) {
        return nullptr;
    }
    if (inProgress.find(languageName) != inProgress.end()) {
        return nullptr;   // inheritance cycle
    }
    inProgress.insert(languageName);

    auto section = indentRoot->GetNode(languageName);
    auto table = std::make_shared<IndentTable>();

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

// Merge one section's declared rules onto the table. Each declared key REPLACES the inherited set (a child
// redeclaring indent_after/dedent_on/suppress_in fully overrides the parent's; absent keys are inherited).
void IndentCache::ApplySection(const ConfigNode &section, IndentTable &table) {
    if (section.HasKey("suppress_in")) {
        table.suppressIn.clear();
        for (auto &name : section.GetSequenceOfStr("suppress_in")) {
            AddSuppressClasses(name, table.suppressIn);
        }
    }
    if (section.HasKey("indent_after")) {
        table.indentAfter.clear();
        for (auto &tok : section.GetSequenceOfStr("indent_after")) {
            if (!tok.empty()) {
                table.indentAfter.insert(FirstChar32(tok));
            }
        }
    }
    if (section.HasKey("dedent_on")) {
        table.dedentOn.clear();
        for (auto &tok : section.GetSequenceOfStr("dedent_on")) {
            if (!tok.empty()) {
                table.dedentOn.insert(FirstChar32(tok));
            }
        }
    }
}

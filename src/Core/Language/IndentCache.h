//
// Created by gnilk on 11.06.26.
//
// Loads indent.yml once and builds a resolved IndentTable per language config-name, with intra-file
// 'inherit' (a 'generic' base). Clone of AutoPairCache: build-once, cache, cycle-guarded. An unknown name
// (or no config at all) yields an EMPTY table (= the engine copies the previous indent, never dedents on
// type). LoadFromString keeps it hermetically testable without the asset loader. See docs/indent-plan.md.
//

#ifndef EDITOR_INDENTCACHE_H
#define EDITOR_INDENTCACHE_H

#include <map>
#include <set>
#include <string>
#include <optional>

#include "Core/Language/IndentTable.h"
#include "Core/Config/ConfigNode.h"

namespace gedit {

    class IndentCache {
    public:
        static IndentCache &Instance();

        // The resolved (inherit-folded) table for a language config name (e.g. "cpp"). Never null - an
        // unknown name returns a shared empty table (copy-previous-indent, no electric dedent).
        IndentTable::Ref GetTableForLanguage(const std::string &languageName);

        // Reset cache + loaded config (reload / test isolation).
        void Clear();

        // Parse the 'indent' root from a YAML string instead of the asset file (tests / explicit load).
        bool LoadFromString(const std::string &yaml);

    private:
        IndentCache() = default;

        bool EnsureLoaded();
        IndentTable::Ref BuildTable(const std::string &languageName, std::set<std::string> &inProgress);
        static void ApplySection(const ConfigNode &section, IndentTable &table);

    private:
        std::optional<ConfigNode> indentRoot;   // the 'indent' node (sections keyed by language name)
        bool didAttemptLoad = false;
        std::map<std::string, IndentTable::Ref> cache;
    };
}

#endif //EDITOR_INDENTCACHE_H

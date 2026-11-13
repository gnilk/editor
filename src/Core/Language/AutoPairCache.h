//
// Created by gnilk on 11.06.26.
//
// Loads autopairs.yml once and builds a resolved AutoPairTable per language config-name, with intra-file
// 'inherit' (a 'generic' base). Mirrors KeyMappingCache: build-once, cache, cycle-guarded. An unknown name
// (or no config at all) yields an EMPTY table (= no pairing). LoadFromString keeps it hermetically testable
// without the asset loader.
//

#ifndef EDITOR_AUTOPAIRCACHE_H
#define EDITOR_AUTOPAIRCACHE_H

#include <map>
#include <set>
#include <string>
#include <optional>

#include "Core/Language/AutoPairTable.h"
#include "Core/Config/ConfigNode.h"

namespace gedit {

    class AutoPairCache {
    public:
        static AutoPairCache &Instance();

        // The resolved (inherit-folded) table for a language config name (e.g. "cpp"). Never null - an
        // unknown name returns a shared empty table (no pairing).
        AutoPairTable::Ref GetTableForLanguage(const std::string &languageName);

        // Reset cache + loaded config (reload / test isolation).
        void Clear();

        // A language file's own section (docs/support-languages.md §3), its chain already folded in. Used for a
        // language the asset file has no section for - until phase 4 the asset file wins. Kept across Clear().
        void SetLanguageFileSection(const std::string &languageName, const ConfigNode &section);

        // Parse the 'autopairs' root from a YAML string instead of the asset file (tests / explicit load).
        bool LoadFromString(const std::string &yaml);

    private:
        AutoPairCache() = default;

        bool EnsureLoaded();
        AutoPairTable::Ref BuildTable(const std::string &languageName, std::set<std::string> &inProgress);
        static void ApplySection(const ConfigNode &section, AutoPairTable &table);

    private:
        std::optional<ConfigNode> autopairsRoot;   // the 'autopairs' node (sections keyed by language name)
        bool didAttemptLoad = false;
        std::map<std::string, AutoPairTable::Ref> cache;
        std::map<std::string, ConfigNode> languageFileSections;
    };
}

#endif //EDITOR_AUTOPAIRCACHE_H

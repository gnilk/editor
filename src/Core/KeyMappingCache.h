//
// Created by gnilk on 08.06.26.
//
// Process-wide cache of built, fully-resolved keymaps. This is the single source of truth for keymap
// loading: both Editor (as a consumer) and KeyMapping inheritance resolution go through here, so each
// named keymap is loaded + built exactly once and shared.
//
// Deliberately NOT owned by Editor - KeyMapping must not depend on Editor. The cache depends only on
// KeyMapping (plus the asset loader, via KeyMapping::LoadKeymapConfig).
//
#ifndef EDITOR_KEYMAPPINGCACHE_H
#define EDITOR_KEYMAPPINGCACHE_H

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "Core/KeyMapping.h"

namespace gedit {
    class KeyMappingCache {
    public:
        static KeyMappingCache &Instance();

        // Return the cached keymap, or load + build it (resolving its 'inherit' chain through this
        // same cache, so ancestors are built once and reused). Returns nullptr if it cannot be built.
        KeyMapping::Ref GetOrLoad(const std::string &name);
        bool Has(const std::string &name) const;
        void Clear();   // drop all cached keymaps (reload / test isolation)

    private:
        std::unordered_map<std::string, KeyMapping::Ref> cache;
        std::unordered_set<std::string> inProgress;   // cycle guard during recursive build
    };
}

#endif //EDITOR_KEYMAPPINGCACHE_H

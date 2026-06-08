//
// Created by gnilk on 08.06.26.
//
#include "Core/KeyMappingCache.h"
#include "logger.h"

using namespace gedit;

KeyMappingCache &KeyMappingCache::Instance() {
    static KeyMappingCache instance;
    return instance;
}

KeyMapping::Ref KeyMappingCache::GetOrLoad(const std::string &name) {
    auto logger = gnilk::Logger::GetLogger("KeyMappingCache");

    auto it = cache.find(name);
    if (it != cache.end()) {
        return it->second;
    }

    // A keymap currently being built that asks for itself (directly or via an ancestor) is a cycle;
    // bail instead of recursing forever. (A->B->A.)
    if (inProgress.find(name) != inProgress.end()) {
        logger->Error("Cyclic keymap inheritance detected while loading '%s' - aborting", name.c_str());
        return nullptr;
    }
    inProgress.insert(name);

    KeyMapping::Ref keymap = nullptr;
    auto cfgNode = KeyMapping::LoadKeymapConfig(name);
    if (!cfgNode.has_value()) {
        logger->Error("Keymap '%s' could not be loaded", name.c_str());
    } else {
        // Resolve any inherited parents through this same cache (built once, then shared).
        KeyMapping::ParentResolver resolver = [this](const std::string &parentName) {
            return GetOrLoad(parentName);
        };
        keymap = KeyMapping::Create(cfgNode.value(), resolver);
        if (keymap == nullptr) {
            logger->Error("Keymap '%s' failed to build", name.c_str());
        }
    }

    inProgress.erase(name);
    if (keymap != nullptr) {
        cache[name] = keymap;
        logger->Debug("Keymap '%s' built and cached", name.c_str());
    }
    return keymap;
}

bool KeyMappingCache::Has(const std::string &name) const {
    return cache.find(name) != cache.end();
}

void KeyMappingCache::Clear() {
    cache.clear();
    inProgress.clear();
}

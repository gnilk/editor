//
// Created by gnilk on 24.07.23.
//
#include <set>

#include "logger.h"
#include "Core/RuntimeConfig.h"

#include "ConfigNode.h"
#include "YAMLMerge.h"
using namespace gedit;

bool ConfigNode::LoadConfig(const std::string &filename, AssetLoaderBase::kLocationType locationType) {
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto configAsset = assetLoader.LoadTextAsset(filename, locationType);
    if (configAsset == nullptr) {
        return false;
    }
    dataNode = YAML::Load(configAsset->GetPtrAs<char *>());

    if (!dataNode.IsDefined()) {
        return false;
    }
    return true;
}

void ConfigNode::MergeNode(ConfigNode &other) {
    YAMLMerge::MergeNode(dataNode, other.dataNode);
}


static const std::string INHERIT_KEY = "inherit";

ConfigNode ConfigNode::ResolveSection(const std::string &name) const {
    // The chain leaf first: name, its parent, the parent's parent...
    std::vector<YAML::Node> chain;
    std::set<std::string> seen;
    auto current = name;
    while (!current.empty()) {
        if (!seen.insert(current).second) {
            gnilk::Logger::GetLogger("Config")->Warn("Section '%s': 'inherit' loops back to '%s', stopped there", name.c_str(), current.c_str());
            break;
        }
        auto section = NodeAtPath(dataNode, {current});
        if (!section.IsDefined()) {
            if (current != name) {
                gnilk::Logger::GetLogger("Config")->Warn("Section '%s': inherits '%s', which doesn't exist", name.c_str(), current.c_str());
            }
            break;
        }
        chain.push_back(section);
        current = (section.IsMap() && section[INHERIT_KEY].IsScalar()) ? section[INHERIT_KEY].Scalar() : "";
    }
    if (chain.empty()) {
        return ConfigNode(YAML::Node(YAML::NodeType::Undefined));
    }
    // The root of the chain is the base; each child is laid over it in turn, the leaf last
    auto resolved = YAML::Clone(chain.back());
    for (auto it = chain.rbegin() + 1; it != chain.rend(); ++it) {
        OverlayNode(resolved, *it);
    }
    if (resolved.IsMap()) {
        resolved.remove(INHERIT_KEY);
    }
    return ConfigNode(resolved);
}

// 'source' over 'target': maps key by key, anything else (a scalar, a list) replaces whole - a child's list is its
// own, never merged by position (open-bugs #28)
void ConfigNode::OverlayNode(YAML::Node target, const YAML::Node &source) {
    if (!source.IsMap() || !target.IsMap()) {
        target = YAML::Clone(source);
        return;
    }
    for (auto const &entry : source) {
        auto key = entry.first.Scalar();
        const YAML::Node &constTarget = target;
        if (constTarget[key].IsDefined() && constTarget[key].IsMap() && entry.second.IsMap()) {
            OverlayNode(target[key], entry.second);
        } else {
            target[key] = YAML::Clone(entry.second);
        }
    }
}

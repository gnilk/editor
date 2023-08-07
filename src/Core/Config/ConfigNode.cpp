//
// Created by gnilk on 24.07.23.
//
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


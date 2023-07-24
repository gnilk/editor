//
// Created by gnilk on 24.07.23.
//
#include "Core/RuntimeConfig.h"

#include "ConfigNode.h"

using namespace gedit;

bool ConfigNode::LoadConfig(const std::string &filename) {
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto configAsset = assetLoader.LoadTextAsset(filename);
    if (configAsset == nullptr) {
        return false;
    }
    dataNode = YAML::Load(configAsset->GetPtrAs<char *>());

    if (!dataNode.IsDefined()) {
        return false;
    }
    return true;
}

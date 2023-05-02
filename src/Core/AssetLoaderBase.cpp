//
// Created by gnilk on 02.05.23.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <filesystem>
#include <fstream>
#include "Core/AssetLoaderBase.h"

using namespace gedit;

AssetLoaderBase::Asset::Ref AssetLoaderBase::LoadAsset(const std::string &relPath) {

    auto path = std::filesystem::path(relPath);
    if (!std::filesystem::exists(path)) {
        return {};
    }
    if (!std::filesystem::is_regular_file(relPath)) {
        return {};
    }

    Asset::Ref asset = std::make_shared<Asset>();
    auto szFile = std::filesystem::file_size(relPath);
    asset->size = szFile;
    asset->ptrData = new unsigned char [szFile];

    std::ifstream inputStream(relPath, std::ios::binary);
    inputStream.read(static_cast<char *>(asset->ptrData), szFile);
    inputStream.close();

    return asset;
}

// This will zero terminate
AssetLoaderBase::Asset::Ref AssetLoaderBase::LoadTextAsset(const std::string &relPath) {

    auto path = std::filesystem::path(relPath);
    if (!std::filesystem::exists(path)) {
        return {};
    }
    if (!std::filesystem::is_regular_file(relPath)) {
        return {};
    }

    Asset::Ref asset = std::make_shared<Asset>();
    auto szFile = std::filesystem::file_size(relPath);
    asset->size = szFile + 1;
    asset->ptrData = new unsigned char [szFile + 1];
    memset(asset->ptrData, 0, szFile+1);

    // Still load as binary to avoid any stupid CRLN conversion - we want the text as-is
    std::ifstream inputStream(relPath, std::ios::binary);
    inputStream.read(static_cast<char *>(asset->ptrData), szFile);
    inputStream.close();

    return asset;
}

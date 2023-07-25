//
// Created by gnilk on 02.05.23.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <filesystem>
#include <fstream>
#include <algorithm>

#include "logger.h"

#include "Core/AssetLoaderBase.h"


using namespace gedit;

void AssetLoaderBase::AddSearchPath(const std::filesystem::path &path) {
    baseSearchPaths.push_back({0, path});
}

AssetLoaderBase::Asset::Ref AssetLoaderBase::LoadAsset(const std::string &fileName) {
    for(auto &searchPath : baseSearchPaths) {
        auto asset = DoLoadAsset(searchPath, fileName);
        if (asset != nullptr) {
            searchPath.score += 1;  // Add a score to this path => higher probability we search it first next time...
            SortSearchPaths();
            return asset;
        }
    }
    return nullptr;
}
AssetLoaderBase::Asset::Ref AssetLoaderBase::LoadTextAsset(const std::string &fileName) {
    for(auto &searchPath : baseSearchPaths) {
        auto asset = DoLoadAsset(searchPath, fileName,1);
        if (asset != nullptr) {
            searchPath.score += 1;
            SortSearchPaths();
            return asset;
        }
    }
    return nullptr;
}

void AssetLoaderBase::SortSearchPaths() {
    std::sort(baseSearchPaths.begin(), baseSearchPaths.end(),[](const SearchPath &a, const SearchPath &b) -> bool {
        return a.score > b.score;
    });
    auto logger = gnilk::Logger::GetLogger("AssetLoader");
    logger->Debug("Resorting paths");
    for(auto &path : baseSearchPaths) {
        logger->Debug("  S=%d, P=%s",path.score, path.path.c_str());
    }
}

//
// TO-DO implement loading strategy - and make it possible for a platform layer to decide..
//
AssetLoaderBase::Asset::Ref AssetLoaderBase::DoLoadAsset(const SearchPath &searchPath, const std::string &fileName, size_t nBytesToAdd) {

    auto pathName = searchPath.path / fileName;
    if (!CheckFilePath(pathName)) {
        return {};
    }

    Asset::Ref asset = std::make_shared<Asset>();
    auto szFile = std::filesystem::file_size(pathName);
    asset->size = szFile;
    asset->ptrData = new unsigned char [szFile + nBytesToAdd];
    memset(asset->ptrData, 0, szFile + nBytesToAdd);

    std::ifstream inputStream(pathName, std::ios::binary);
    inputStream.read(static_cast<char *>(asset->ptrData), szFile);
    inputStream.close();

    auto logger = gnilk::Logger::GetLogger("AssetLoader");
    logger->Info("Ok loaded %zu bytes from '%s'",szFile, pathName.c_str());


    return asset;
}

bool AssetLoaderBase::CheckFilePath(const std::filesystem::path filePath) {
    if (!std::filesystem::exists(filePath)) {
        auto logger = gnilk::Logger::GetLogger("AssetLoader");
        logger->Error("File not found: '%s'",filePath.c_str());
        return {};
    }
    if (!std::filesystem::is_regular_file(filePath)) {
        auto logger = gnilk::Logger::GetLogger("AssetLoader");
        logger->Error("File is not a regular file: '%s'",filePath.c_str());
        return {};
    }
    return true;
}

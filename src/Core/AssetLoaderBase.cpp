//
// Created by gnilk on 02.05.23.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <unordered_map>

#include "logger.h"

#include "Core/AssetLoaderBase.h"
#include <unordered_map>

using namespace gedit;

static std::unordered_map<AssetLoaderBase::kLocationType, std::string> locTypeToString = {
        { AssetLoaderBase::kLocationType::kAny, "any"},
        { AssetLoaderBase::kLocationType::kSystem, "system"},
        { AssetLoaderBase::kLocationType::kUser, "user"}
};

bool AssetLoaderBase::AddSearchPath(const std::filesystem::path &path, kLocationType locationType) {
    auto logger = gnilk::Logger::GetLogger("AssetLoader");
    if (!std::filesystem::is_directory(path)) {
        logger->Debug("Path '%s' is not directory, skipping!", path.c_str());
        return false;
    }
    logger->Debug("AddSearchPath (%s): %s",locTypeToString[locationType].c_str(), path.c_str());

    if (path.is_relative()) {
        logger->Debug("Path '%s' is relative", path.c_str());
    }
    auto absPath = std::filesystem::absolute(path);
    logger->Debug("Adding absolute: '%s'", absPath.c_str());
    baseSearchPaths.push_back({locationType, absPath});
    SortSearchPaths();
    return true;
}

bool AssetLoaderBase::ReplaceSearchPath(const std::filesystem::path &path, kLocationType locationType) {
    // Drop any previously registered paths of this location type, then add the new one (re-sorts).
    baseSearchPaths.erase(
        std::remove_if(baseSearchPaths.begin(), baseSearchPaths.end(),
                       [locationType](const SearchPath &sp) { return sp.locationType == locationType; }),
        baseSearchPaths.end());
    return AddSearchPath(path, locationType);
}

AssetLoaderBase::Asset::Ref AssetLoaderBase::LoadAsset(const std::string &fileName, kLocationType locationType) {
    for(auto &searchPath : baseSearchPaths) {
        if ((searchPath.locationType == locationType) || (locationType == kLocationType::kAny)) {
            auto asset = DoLoadAsset(searchPath, fileName);
            if (asset != nullptr) {
                return asset;
            }
        }
    }
    return nullptr;
}
AssetLoaderBase::Asset::Ref AssetLoaderBase::LoadTextAsset(const std::string &fileName, kLocationType locationType) {
    for(auto &searchPath : baseSearchPaths) {
        if ( (searchPath.locationType == locationType) || (locationType == kLocationType::kAny)) {
            auto asset = DoLoadAsset(searchPath, fileName, 1);
            if (asset != nullptr) {
                return asset;
            }
        }
    }
    return nullptr;
}

std::filesystem::path AssetLoaderBase::ResolveWritePath(const std::string &fileName, kLocationType locationType) const {
    // NOTE: within a location type baseSearchPaths keeps registration order, so "first matching" is the
    // preferred path of this type (e.g. the XDG config dir before the legacy user dirs).
    for (auto &searchPath : baseSearchPaths) {
        if (searchPath.locationType == locationType) {
            return searchPath.path / fileName;
        }
    }
    return {};
}

std::filesystem::path AssetLoaderBase::ResolveExistingPath(const std::string &fileName, kLocationType locationType) const {
    for (auto &searchPath : baseSearchPaths) {
        if ((searchPath.locationType != locationType) && (locationType != kLocationType::kAny)) {
            continue;
        }
        std::error_code ec;
        auto candidate = searchPath.path / fileName;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

std::vector<std::filesystem::path> AssetLoaderBase::ResolveExistingDirectories(const std::string &dirName, kLocationType locationType) const {
    std::vector<std::filesystem::path> directories;
    for (auto &searchPath : baseSearchPaths) {
        if ((searchPath.locationType != locationType) && (locationType != kLocationType::kAny)) {
            continue;
        }
        std::error_code ec;
        auto candidate = searchPath.path / dirName;
        if (std::filesystem::is_directory(candidate, ec)) {
            directories.push_back(candidate);
        }
    }
    return directories;
}

static int locationPriority(AssetLoaderBase::kLocationType t) {
    switch (t) {
        case AssetLoaderBase::kLocationType::kSystem:  return 0;
        case AssetLoaderBase::kLocationType::kUser:    return 1;
        case AssetLoaderBase::kLocationType::kAny:     return 2;
        // Conservative default: project paths are searched LAST in a kAny load, so a
        // project-local copy is only a fallback and never overrides user/system assets.
        // Flip this below kUser/kSystem if project-local OVERRIDE semantics are wanted.
        case AssetLoaderBase::kLocationType::kProject: return 3;
    }
    return 3;
}

void AssetLoaderBase::SortSearchPaths() {
    // Stable: within a location type the registration order is the priority order - the caller registers
    // the preferred directory first, and a hit in a fallback must never promote it above that.
    std::stable_sort(baseSearchPaths.begin(), baseSearchPaths.end(), [](const SearchPath &a, const SearchPath &b) -> bool {
        return locationPriority(a.locationType) < locationPriority(b.locationType);
    });
}

//
// TO-DO implement loading strategy - and make it possible for a platform layer to decide..
//
AssetLoaderBase::Asset::Ref AssetLoaderBase::DoLoadAsset(const SearchPath &searchPath, const std::string &fileName, size_t nBytesToAdd) {

    auto pathName = searchPath.path / fileName;
    if (!CheckFilePath(pathName)) {
        return {};
    }
    auto logger = gnilk::Logger::GetLogger("AssetLoader");
    logger->Debug("Loading Asset: %s", pathName.c_str());


    Asset::Ref asset = Asset::Create(pathName);
    auto szFile = std::filesystem::file_size(pathName);
    asset->size = szFile;
    asset->ptrData = new unsigned char [szFile + nBytesToAdd];
    memset(asset->ptrData, 0, szFile + nBytesToAdd);

    std::ifstream inputStream(pathName, std::ios::binary);
    inputStream.read(static_cast<char *>(asset->ptrData), szFile);
    inputStream.close();

//    auto logger = gnilk::Logger::GetLogger("AssetLoader");
//    logger->Info("Ok loaded %zu bytes from '%s'",szFile, pathName.c_str());


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

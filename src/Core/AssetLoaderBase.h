//
// Created by gnilk on 02.05.23.
//

#ifndef EDITOR_ASSETLOADERBASE_H
#define EDITOR_ASSETLOADERBASE_H

#include <memory>
#include <optional>
#include <string>

namespace gedit {
    class AssetLoaderBase {
    public:
        struct Asset {
            using Ref = std::shared_ptr<Asset>;
            size_t size = 0;
            void *ptrData = nullptr;
        };
    public:
        // Will load as-is
        Asset::Ref LoadAsset(const std::string &relPath);
        // Will zero terminate
        Asset::Ref LoadTextAsset(const std::string &relPath);
    };
}


#endif //EDITOR_ASSETLOADERBASE_H

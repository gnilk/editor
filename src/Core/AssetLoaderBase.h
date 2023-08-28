//
// Created by gnilk on 02.05.23.
//

#ifndef EDITOR_ASSETLOADERBASE_H
#define EDITOR_ASSETLOADERBASE_H

#include <memory>
#include <optional>
#include <string>
#include <filesystem>
#include <vector>

namespace gedit {
    class AssetLoaderBase {
    public:
        enum class kLocationType {
            kAny,
            kSystem,
            kUser,
        };
    public:
        class Asset {
            friend AssetLoaderBase;
        public:
            using Ref = std::shared_ptr<Asset>;
        public:
            Asset() = default;
            virtual ~Asset() {
                if (ptrData != nullptr) {
                    delete static_cast<unsigned char *>(ptrData);
                }
            }
            size_t GetSize() {
                return size;
            }
            template<typename T>
                T GetPtrAs() {
                    //std::is_pointer<T>();
                    return static_cast<T>(ptrData);
                }
        protected:
            size_t size = 0;
            void *ptrData = nullptr;
        };
    private:
        struct SearchPath {
            int score = 0;
            kLocationType locationType;
            std::filesystem::path path;
        };
    public:
        bool AddSearchPath(const std::filesystem::path &path, kLocationType locationType);

        // Will load as-is
        Asset::Ref LoadAsset(const std::string &fileName, kLocationType locationType = kLocationType::kAny);

        // Will zero terminate
        Asset::Ref LoadTextAsset(const std::string &fileName, kLocationType locationType = kLocationType::kAny);


    protected:
        void SortSearchPaths();
        Asset::Ref DoLoadAsset(const SearchPath &searchPath, const std::string &fileName, size_t nBytesToAdd = 0);
        bool CheckFilePath(const std::filesystem::path filePath);
    protected:
        std::vector<SearchPath> baseSearchPaths;
    };
}


#endif //EDITOR_ASSETLOADERBASE_H

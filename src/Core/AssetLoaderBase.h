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
            // The origin path is mandatory at construction - an asset always knows where
            // it came from, so save-back has a target. Go through Create() to make one.
            explicit Asset(std::filesystem::path originPath) : originPath(std::move(originPath)) {}
            virtual ~Asset() {
                if (ptrData != nullptr) {
                    delete[] static_cast<unsigned char *>(ptrData);
                }
            }
            static Ref Create(const std::filesystem::path &originPath) {
                return std::make_shared<Asset>(originPath);
            }
            size_t GetSize() {
                return size;
            }
            const std::filesystem::path &GetOriginPath() const {
                return originPath;
            }
            template<typename T>
                T GetPtrAs() {
                    //std::is_pointer<T>();
                    return static_cast<T>(ptrData);
                }
        protected:
            size_t size = 0;
            void *ptrData = nullptr;
            std::filesystem::path originPath = {};
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

        // Resolve where a file should be WRITTEN for the given location, independent of
        // whether it currently exists. Needed for save-back of assets that may not exist
        // on first run (e.g. history). Returns the first registered search path of that
        // location type, or empty if none qualifies.
        std::filesystem::path ResolveWritePath(const std::string &fileName, kLocationType locationType) const;

    protected:
        void SortSearchPaths();
        Asset::Ref DoLoadAsset(const SearchPath &searchPath, const std::string &fileName, size_t nBytesToAdd = 0);
        bool CheckFilePath(const std::filesystem::path filePath);
    protected:
        std::vector<SearchPath> baseSearchPaths;
    };
}


#endif //EDITOR_ASSETLOADERBASE_H

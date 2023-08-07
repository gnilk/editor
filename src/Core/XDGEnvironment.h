//
// Created by gnilk on 07.08.23.
//

#ifndef EDITOR_XDGENVIRONMENT_H
#define EDITOR_XDGENVIRONMENT_H

#include <string>
#include <filesystem>
#include <utility>
#include <vector>
#include <optional>

namespace gedit {
    class XDGEnvironment {
    public:
        virtual ~XDGEnvironment() = default;
        static const XDGEnvironment &Instance();

        [[nodiscard]]
        const std::filesystem::path &GetUserDataPath() const;
        [[nodiscard]]
        const std::filesystem::path &GetUserConfigPath() const;
        [[nodiscard]]
        const std::filesystem::path &GetUserCachePath() const;
        [[nodiscard]]
        const std::filesystem::path &GetUserStatePath() const;
        [[nodiscard]]
        const std::filesystem::path &GetUserRuntimePath() const;

        [[nodiscard]]
        const std::vector<std::filesystem::path> &GetSystemConfigPaths() const;
        [[nodiscard]]
        const std::vector<std::filesystem::path> &GetSystemDataPaths() const;

        [[nodiscard]]
        const std::filesystem::path &GetFirstSystemDataPath() const;
        [[nodiscard]]
        std::optional<const std::filesystem::path>GetFirstSystemDataPathWithPrefix(const std::string &prefix) const;

        [[nodiscard]]
        const std::filesystem::path &GetFirstSystemConfigPath() const;
        [[nodiscard]]
        std::optional<const std::filesystem::path>GetFirstSystemConfigPathWithPrefix(const std::string &prefix) const;

        void Reset();

    private:
        // Could consolidate the below to this...
        struct PathVar {
            bool isDefault = false;
            std::string envValue;
            std::filesystem::path fsPath;   // consider making this optional.
        };
        struct PathList {
            bool isDefault = false;
            std::string envValue;
            std::vector<std::filesystem::path> fsPaths;
        };


        XDGEnvironment() = default;
        bool Initialize();
        [[nodiscard]]
        bool IsInitialized() const {
            return isInitialized;
        }
        bool ResolvePathAndVar(PathVar &pathVar, const std::string &var, const std::string &defaultValue);
        static bool ResolvePathListFromVar(PathList &pathList, const std::string &var, const std::string &defaultValue);

        static std::pair<std::string, bool> GetEnv(const std::string &var, const std::string &defaultValue);
        std::filesystem::path PathFromVar(const std::string &var);
        static bool PathListFromVar(std::vector<std::filesystem::path> &outList, const std::string &varValue);
    private:
    private:
        bool isInitialized = false;

        // Raw variables
        std::string userHome = {};
        std::filesystem::path pathHome = {};

        // User variables - these should all have paths relative $HOME
        PathVar userData = {};
        PathVar userState = {};
        PathVar userConfig = {};
        PathVar userCache = {};
        PathVar userRuntime = {};

        // System config variables
        PathList sysConfig = {};
        PathList sysData = {};
    };
}


#endif //EDITOR_XDGENVIRONMENT_H

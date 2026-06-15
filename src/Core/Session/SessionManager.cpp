//
// Created by gnilk on 15.06.26.
//

#include <fcntl.h>
#include <unistd.h>
#include <filesystem>
#include <string>

#include "Core/Session/SessionManager.h"
#include "Core/Session/SessionSerializer.h"
#include "Core/RuntimeConfig.h"
#include "Core/AssetLoaderBase.h"

using namespace gedit;

// Crash-safe write: stage to <file>.tmp, fsync, then rename over the target (rename is atomic on a POSIX
// filesystem). A crash mid-write leaves the previous file intact; never a half-written session.
static bool AtomicWrite(const std::filesystem::path &path, const std::string &content) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);   // defensive; kProject dir normally exists

    std::filesystem::path tmpPath = path;
    tmpPath += ".tmp";

    int fd = ::open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return false;
    }
    size_t remaining = content.size();
    const char *ptr = content.data();
    while (remaining > 0) {
        ssize_t n = ::write(fd, ptr, remaining);
        if (n < 0) {
            ::close(fd);
            ::unlink(tmpPath.c_str());
            return false;
        }
        ptr += n;
        remaining -= static_cast<size_t>(n);
    }
    ::fsync(fd);
    ::close(fd);

    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }
    return true;
}

SessionManager &SessionManager::Instance() {
    static SessionManager glbSessionManager;
    if (glbSessionManager.logger == nullptr) {
        glbSessionManager.logger = gnilk::Logger::GetLogger("SessionManager");
    }
    return glbSessionManager;
}

SessionManager::SessionManager() {
}

void SessionManager::Clear() {
    currentSession = {};
    isLoaded = false;
}

bool SessionManager::Load() {
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto asset = assetLoader.LoadTextAsset("session.yml", AssetLoaderBase::kLocationType::kProject);
    if (asset == nullptr) {
        // No session file yet (first run for this project, or no project open) - start clean.
        return false;
    }
    std::string text(asset->GetPtrAs<const char *>());
    RootSession parsed;
    if (!SessionSerializer::FromYaml(text, parsed)) {
        logger->Warning("Load: session present but unparseable / newer version - starting clean");
        return false;
    }
    currentSession = parsed;
    isLoaded = true;
    return true;
}

bool SessionManager::Save() {
    auto &assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto path = assetLoader.ResolveWritePath("session.yml", AssetLoaderBase::kLocationType::kProject);
    if (path.empty()) {
        // No kProject write-path registered => no project open => nothing to persist.
        logger->Debug("Save: no kProject write-path registered - skipping");
        return false;
    }
    auto text = SessionSerializer::ToYaml(currentSession);
    if (!AtomicWrite(path, text)) {
        logger->Error("Save: failed writing session to '%s'", path.string().c_str());
        return false;
    }
    logger->Debug("Save: wrote session to '%s'", path.string().c_str());
    return true;
}

void SessionManager::OnFolderOpened(const std::filesystem::path &root) {
    // TODO(phase1): folder-open trigger (is_directory branch only). Create .goatedit/ when in-home and
    //               auto_create_project_dir is set, (re)register kProject, Load(root), then apply restore.
    logger->Debug("OnFolderOpened: not yet implemented (root=%s)", root.string().c_str());
}

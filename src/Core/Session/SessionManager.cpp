//
// Created by gnilk on 15.06.26.
//

#include "Core/Session/SessionManager.h"

using namespace gedit;

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

bool SessionManager::Load(const std::filesystem::path &root) {
    // TODO(phase1): read <root>/.goatedit/session.yml via assetLoader.LoadAsset("session.yml", kProject),
    //               parse the YAML into currentSession, set isLoaded. Tolerate missing/corrupt/newer.
    logger->Debug("Load: not yet implemented (root=%s)", root.string().c_str());
    return false;
}

bool SessionManager::Save(const std::filesystem::path &root) {
    // TODO(phase1): atomic-write currentSession to <root>/.goatedit/session.yml via
    //               assetLoader.ResolveWritePath("session.yml", kProject) (tmp -> fsync -> rename).
    logger->Debug("Save: not yet implemented (root=%s)", root.string().c_str());
    return false;
}

void SessionManager::OnFolderOpened(const std::filesystem::path &root) {
    // TODO(phase1): folder-open trigger (is_directory branch only). Create .goatedit/ when in-home and
    //               auto_create_project_dir is set, (re)register kProject, Load(root), then apply restore.
    logger->Debug("OnFolderOpened: not yet implemented (root=%s)", root.string().c_str());
}

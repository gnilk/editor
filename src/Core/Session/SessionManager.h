//
// Created by gnilk on 15.06.26.
//
// SessionManager — orchestrator + sole owner of session-cache disk I/O (docs/session-cache.md §4).
//
// Two locked invariants:
//   1. SessionManager is the ONLY component that reads/writes a session file. Subsystems (incl. the
//      rendering backend) exchange in-memory DTOs (SessionState) with it — they never touch disk.
//   2. Reachability is the singleton SessionManager::Instance() (mirrors Config / KeyMappingCache),
//      loaded during Editor::Initialize so it is populated before OpenScreen() creates the window.
//      It is NOT registered in RuntimeConfig.
//
// Phase 1 = per-root persistence only (no registry, no respawn, no undo, no index). The methods below
// are scaffolding stubs; placement is via AssetLoader kProject (<root>/.goatedit/session.yml).
//

#ifndef EDITOR_SESSIONMANAGER_H
#define EDITOR_SESSIONMANAGER_H

#include <filesystem>
#include <functional>
#include <logger.h>

#include "Core/Session/SessionState.h"
#include "Core/Timer.h"

namespace gedit {

    class SessionManager {
    public:
        static SessionManager &Instance();

        // Reset in-memory state — test isolation (mirrors KeyMappingCache::Clear).
        void Clear();

        // Per-root persistence (Phase 1). Both resolve the file through AssetLoader kProject
        // (<cwd>/.goatedit/session.yml); Save writes atomically (tmp -> fsync -> rename). SessionManager
        // stays path-policy-agnostic - placement/auto-create live at the kProject registration (§4.4).
        // Load returns false (caller starts clean) when there is no session file or it can't be parsed.
        bool Load();
        bool Save();

        // Trigger: called from the folder-open path (the is_directory branch ONLY — a single-file open
        // is sessionless, §3.3). Creates .goatedit/ when allowed, loads the session, applies restore.
        void OnFolderOpened(const std::filesystem::path &root);

        // Debounced autosave (§4.3). NotifyChanged() is called from meaningful events (doc open/close,
        // active-doc switch, splitter move, window resize/move); it (re)starts a debounce timer so a
        // burst of events coalesces into a single save. On elapse the injected handler is posted to the
        // main-thread runloop (Save walks the live view tree, so it must run there). The handler is set
        // by Editor (SetAutoSaveHandler) to keep SessionManager free of an Editor dependency; without a
        // handler (tests / headless) NotifyChanged is a no-op.
        void SetAutoSaveHandler(std::function<void()> handler);
        void NotifyChanged();

        // In-memory view of the current root's session. Callers (e.g. the backend reading geometry, or
        // a subsystem reporting its state) mutate this; SessionManager owns persisting it.
        RootSession &CurrentSession() {
            return currentSession;
        }
        const RootSession &CurrentSession() const {
            return currentSession;
        }

    private:
        SessionManager();

        gnilk::Logger::ILogger *logger = nullptr;
        RootSession currentSession = {};
        bool isLoaded = false;

        // Debounced autosave: handler injected by Editor + the debounce timer (created lazily).
        std::function<void()> autoSaveHandler = nullptr;
        Timer::Ref autoSaveTimer = nullptr;
    };

}

#endif //EDITOR_SESSIONMANAGER_H

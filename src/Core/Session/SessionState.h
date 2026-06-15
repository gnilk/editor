//
// Created by gnilk on 15.06.26.
//
// SessionState — plain DTOs for the per-root session cache (docs/session-cache.md §4, §11.2).
// These are serialisation value types only: no logic, no ownership. SessionManager fills them from
// the live model on save and seeds the model from them on restore. Each subsystem owns its own
// ToSession/FromSession that reads/writes the relevant struct here.
//
// Coordinates follow the model convention: cursor/selection are CHAR INDICES in TextBuffer space
// (the view translates to visual columns at draw time).
//

#ifndef EDITOR_SESSIONSTATE_H
#define EDITOR_SESSIONSTATE_H

#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

#include "Core/DocumentViewMode.h"   // DocumentViewMode (leaf header — no cycle back to DocumentViewState)

namespace gedit {

    // One persistable splitter, addressed by a stable id (e.g. "split.workspace", "split.terminal").
    // Stored both absolute and relative (ratio of content extent) so a restore onto a different window
    // size lands sensibly; restore prefers the relative value and clamps at the SetSplitterPos chokepoint.
    struct SplitterSession {
        std::string id;
        int absolute = 0;
        float relative = 0.0f;
    };

    // Pixel window geometry. Defaults are sentinel-zero == "no stored geometry, use the cold-start
    // default" (centred fraction of the display, decided per docs/session-cache.md §4.2).
    struct WindowGeometrySession {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        bool IsValid() const {
            return (width > 0) && (height > 0);
        }
    };

    // Layout: window geometry + the persistable splitters + which top-view held focus.
    struct LayoutSession {
        WindowGeometrySession window = {};
        std::vector<SplitterSession> splitters = {};
        std::string focusedTopView = {};
    };

    // Active selection, in TextBuffer (char-index) coordinates.
    struct SelectionSession {
        bool isActive = false;
        int startX = 0;
        int startY = 0;
        int endX = 0;
        int endY = 0;
    };

    // One open document ("tab"): file identity + the per-view editing state (cursor/scroll/selection/
    // view-mode). Mirrors DocumentViewState (LineCursor + Selection + DocumentViewMode).
    struct DocumentSession {
        std::string path = {};
        // LineCursor.cursor
        int cursorX = 0;
        int cursorY = 0;
        int wantedColumn = 0;
        // LineCursor scroll window
        std::size_t idxActiveLine = 0;
        int viewTopLine = 0;
        int viewBottomLine = 0;
        SelectionSession selection = {};
        DocumentViewMode viewMode = DocumentViewMode::kText;
    };

    // The whole per-root session: everything needed to bring one project back as it was left.
    // Serialised to <root>/.goatedit/session.yml.
    struct RootSession {
        // Bumped on any incompatible format change; unknown/newer is ignored (start clean, never crash).
        static constexpr int kVersion = 1;

        int version = kVersion;
        std::filesystem::path primaryRoot = {};
        // Other roots open in the same instance (§3.2, multi-root — field present, wiring deferred to Step 2).
        std::vector<std::string> extraRoots = {};
        std::vector<DocumentSession> documents = {};
        int activeDocumentIndex = -1;
        // WorkspaceView tree expand/collapse, keyed by node path string (consolidates the in-tree cache).
        std::unordered_map<std::string, bool> expandCollapse = {};
        LayoutSession layout = {};
    };

}

#endif //EDITOR_SESSIONSTATE_H

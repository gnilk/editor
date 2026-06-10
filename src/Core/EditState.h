//
// Created by gnilk on 09.06.26.
//
// EditState — the per-document edit history (undo). Pulled out of Document (Phase 2, the
// buffer/window split) and held by Ref so that multiple views of the same buffer share ONE undo
// history while each keeps its own cursor/selection (see DocumentViewState.h). Today it is just the
// UndoHistory; the class boundary is what lets the eventual multi-view sharing be a re-point rather
// than a redesign, and makes the undo stack a serializable unit for restore-on-reopen later.
//

#ifndef EDITOR_EDITSTATE_H
#define EDITOR_EDITSTATE_H

#include <memory>
#include "Core/UndoHistory.h"

namespace gedit {

    // Per-document edit state. Held by Document as a Ref (referenced, not fused).
    class EditState {
    public:
        using Ref = std::shared_ptr<EditState>;
        static Ref Create() {
            return std::make_shared<EditState>();
        }
    public:
        UndoHistory historyBuffer;
    };
}

#endif //EDITOR_EDITSTATE_H

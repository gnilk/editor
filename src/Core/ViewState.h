//
// Created by gnilk on 09.06.26.
//
// ViewState — the per-view editing state for a document: the live cursor and the active
// selection. Pulled out of Document (Phase 2, the buffer/window split) so that multiple views
// of the same buffer can each keep their own cursor/selection. Today a Document references
// exactly one ViewState; when the editor gains split/side-by-side views each view-item will own
// its own, and the Document will keep only a saved snapshot. Drawing the class boundary now keeps
// that future move a re-point, not a rewrite.
//

#ifndef EDITOR_VIEWSTATE_H
#define EDITOR_VIEWSTATE_H

#include <memory>
#include <cstddef>
#include "Core/Point.h"
#include "Core/Graphics/Cursor.h"

namespace gedit {

    // NOTE: Selection coordinates are in TextBuffer coordinates!!!!!
    struct Selection {
        bool IsSelected(int x, int y) {
            if (!isActive) return false;
            if (y < startPos.y) return false;
            if (y > endPos.y) return false;

            if ((y == startPos.y) && (x < startPos.x)) return false;
            if ((y == endPos.y) && (x > endPos.x)) return false;

            return true;
        }

        bool IsLineSelected(int y) {
            return IsSelected(0, y);
        }

        bool IsActive() const {
            return isActive;
        }
        size_t GetStartLine() const {
            // Did we select backwards???
            if (endPos.y > startPos.y) {
                return startPos.y;
            }
            return endPos.y;
        }
        size_t GetStartLine() {
            // Did we select backwards???
            if (endPos.y > startPos.y) {
                return startPos.y;
            }
            return endPos.y;
        }

        //
        // This returns the coords sorted!!!
        //
        const Point &GetStart() {
            if (endPos > startPos) {
                return startPos;
            }
            return endPos;
        }

        const Point &GetStart() const {
            if (endPos > startPos) {
                return startPos;
            }
            return endPos;
        }

        const Point &GetEnd() {
            if (endPos < startPos) {
                return startPos;
            }
            return endPos;
        }

        const Point &GetEnd() const {
            if (endPos < startPos) {
                return startPos;
            }
            return endPos;
        }
        // Setters
        void SetStart(const Point &pt) {
            startPos = pt;
        }
        void SetStartYPos(const size_t yp) {
            startPos.y = yp;
        }
        void SetEndYPos(const size_t yp) {
            endPos.y = yp;
        }
        void SetEnd(const Point &pt) {
            endPos = pt;
        }
        void SetStartLine(const size_t startLine) {
            idxStartLine = startLine;
        }
        void SetActive(const bool newActive) {
            isActive = newActive;
        }



    private:

        bool isActive = false;
        size_t idxStartLine;
        Point startPos = {};    // buffer coords
        Point endPos = {};      // buffer coords

    };

    // Per-view editing state. Held by Document as a Ref (referenced, not fused) so it can later be
    // owned by a view-item instead, with the Document keeping a saved snapshot.
    class ViewState {
    public:
        using Ref = std::shared_ptr<ViewState>;
        static Ref Create() {
            return std::make_shared<ViewState>();
        }
    public:
        LineCursor lineCursor;
        Selection currentSelection = {};
    };
}

#endif //EDITOR_VIEWSTATE_H

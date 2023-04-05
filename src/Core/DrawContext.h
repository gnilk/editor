//
// Created by gnilk on 22.02.23.
//

#ifndef NCWIN_DRAWCONTEXT_H
#define NCWIN_DRAWCONTEXT_H

#include <vector>

#include "Core/NativeWindow.h"
#include "Core/Line.h"
#include "Core/Rect.h"
#include "Core/Point.h"
#include "Core/TextAttributes.h"

namespace gedit {

    // TO DO
    // - Support multiple overlays with and 'id' (application defined)
    // - ability to loop over overlays through a lambda filtering on id's (or all)
    // - this allows us to have transform loops for overlays depending on various functions (search results, refactoring, etc..)
    class DrawContext {
    public:
        struct Overlay {
            bool isActive = false;

            // Editor set's these based on text-buffer coordinates
            // where y = row, and x = column...
            // We need to transform these to screen..
            Point start;
            Point end;

            void Set(const Point &newStart, const Point &newEnd) {
                start = newStart;
                end = newEnd;
                // Swap...
                if (start.y > end.y) {
                    start = newEnd;
                    end = newStart;
                }
            }

            bool IsInside(int x, int y) const {
                if (!isActive) return false;
                if (y < start.y) return false;
                if (y > end.y) return false;

                if ((y == start.y) && (x < start.x)) return false;
                if ((y == end.y) && (x >= end.x)) return false;

                return true;
            }

            bool IsInsideLine(int y) const {
                return IsInside(0, y);
            }

            int attributes; // let's see...
        };
    public:
        DrawContext() = default;
        explicit DrawContext(NativeWindow window, Rect clientRect) : win(window), rect(clientRect) {
        }
        virtual ~DrawContext() = default;

        virtual void ClearLine(int y) const {}
        virtual void FillLine(int y, kTextAttributes attrib, char c) const {}

        virtual void Clear() const {}
        virtual void Scroll(int nRows) const {}

        virtual void DrawStringAt(int x, int y, const char *str) const {}
        virtual void DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) const {}
        virtual void DrawStringWithAttributesAndColAt(int x, int y, kTextAttributes attrib, int idxColor, const char *str) const {}

        void SetOverlay(const Overlay &newOverlay) {
            overlay = newOverlay;
            overlay.isActive = true;
        }

        void RemoveOverlay() {
            overlay.isActive = false;
        }

        virtual const Rect &GetRect() const {
            return rect;
        }
    protected:
        Rect rect = {};
        NativeWindow win = nullptr;
        // probably need a list of overlays - like highlighting search results and similar...
        Overlay overlay = {};
    };
}
#endif //NCWIN_DRAWCONTEXT_H

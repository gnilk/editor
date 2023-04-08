//
// Created by gnilk on 31.03.23.
//

#ifndef EDITOR_SDLCURSOR_H
#define EDITOR_SDLCURSOR_H
#include "Core/Cursor.h"
#include <functional>

namespace gedit {
    class SDLCursor {
    public:
        using DrawDelegate = std::function<void(const Cursor &cursor)>;
    public:

        static SDLCursor &Instance() {
            static SDLCursor glbInstance;
            return glbInstance;
        }

        void SetCursor(const Cursor &newCursor, DrawDelegate drawHandler) {
            cursor = newCursor;
            cbDrawCursor = drawHandler;
        }

        const Cursor &GetCursor() {
            return cursor;
        }
        void Draw() {
            if (!cbDrawCursor) return;
            cbDrawCursor(cursor);
        }
    private:
        SDLCursor() = default;
    private:
        Cursor cursor;
        DrawDelegate cbDrawCursor = nullptr;
    };
}

#endif //EDITOR_SDLCURSOR_H

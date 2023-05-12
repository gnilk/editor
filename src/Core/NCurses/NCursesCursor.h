//
// Created by gnilk on 31.03.23.
//

#ifndef GEDIT_NCURSESCURSOR_H
#define GEDIT_NCURSESCURSOR_H
#include "Core/Cursor.h"
#include <functional>

namespace gedit {
    class NCursesCursor {
    public:
        using DrawDelegate = std::function<void(const Cursor &cursor)>;
    public:

        static NCursesCursor &Instance() {
            static NCursesCursor glbInstance;
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
        NCursesCursor() = default;
    private:
        Cursor cursor;
        DrawDelegate cbDrawCursor = nullptr;
    };
}

#endif //EDITOR_SDLCURSOR_H

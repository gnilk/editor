//
// Created by gnilk on 15.04.23.
//

#ifndef EDITOR_VERTICALNAVIGATIONVIEWMODEL_H
#define EDITOR_VERTICALNAVIGATIONVIEWMODEL_H

#include <stdint.h>
#include <stdlib.h>

#include "Cursor.h"
#include "Rect.h"

namespace gedit {
    class VerticalNavigationViewModel {
    public:
        VerticalNavigationViewModel() = default;
        virtual ~VerticalNavigationViewModel() = default;

        void HandleResize(Cursor &cursor, const Rect &viewRect);

        void OnNavigateDownVSCode(Cursor &cursor, size_t rowsToMove, const Rect &viewRect, size_t nItems);
        void OnNavigateUpVSCode(Cursor &cursor, size_t rowsToMove, const Rect &viewRect, size_t nItems);
        void OnNavigateDownCLion(Cursor &cursor, size_t rowsToMove, const Rect &viewRect, size_t nItems);
        void OnNavigateUpCLion(Cursor &cursor, size_t rowsToMove, const Rect &viewRect,  size_t nItems);

    public:
        size_t idxActiveLine = 0;       // can't be negative
        size_t viewTopLine = 0;
        size_t viewBottomLine = 0;
    };
}

#endif //EDITOR_VERTICALNAVIGATIONVIEWMODEL_H

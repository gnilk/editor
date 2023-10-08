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

        void HandleResize(const Rect &viewRect);
        void OnNavigateDownVSCode(size_t rowsToMove, const Rect &viewRect, size_t nItems);
        void OnNavigateUpVSCode(size_t rowsToMove, const Rect &viewRect, size_t nItems);
        void OnNavigateDownCLion(size_t rowsToMove, const Rect &viewRect, size_t nItems);
        void OnNavigateUpCLion(size_t rowsToMove, const Rect &viewRect,  size_t nItems);

    public:
        LineCursor::Ref lineCursor = nullptr;
    };
}

#endif //EDITOR_VERTICALNAVIGATIONVIEWMODEL_H

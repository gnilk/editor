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

        void OnNavigateDownVSCode(Cursor &cursor, int rows, const Rect &rect, const size_t nItems);
        void OnNavigateUpVSCode(Cursor &cursor, int rows, const Rect &rect, const size_t nItems);
        void OnNavigateDownCLion(Cursor &cursor, int rows, const Rect &rect, const size_t nItems);
        void OnNavigateUpCLion(Cursor &cursor, int rows, const Rect &rect, const size_t nItems);

    public:
        int idxActiveLine = 0;
        int32_t viewTopLine = 0;
        int32_t viewBottomLine = 0;
    };
}

#endif //EDITOR_VERTICALNAVIGATIONVIEWMODEL_H

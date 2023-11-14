//
// Created by gnilk on 15.04.23.
//

#ifndef EDITOR_VERTICALNAVIGATIONVIEWMODEL_H
#define EDITOR_VERTICALNAVIGATIONVIEWMODEL_H

#include <stdint.h>
#include <stdlib.h>

#include "Cursor.h"
#include "Rect.h"

#include <memory>

namespace gedit {
    // TODO: the following is more or less needed
    //  1) Split this - one instance for 'CLion' and one for 'VSCode' - checks only where we instance them
    //  2) Introduce a 'clipping' routine - which can be called after the lineCursor has been updated
    //
    class VerticalNavigationViewModel {
    public:
        using Ref = std::unique_ptr<VerticalNavigationViewModel>;
    public:
        VerticalNavigationViewModel() = default;
        virtual ~VerticalNavigationViewModel() = default;

        void HandleResize(const Rect &viewRect);

        virtual void OnNavigateDown(size_t rowsToMove, const Rect &viewRect, size_t nItems);
        virtual void OnNavigateUp(size_t rowsToMove, const Rect &viewRect,  size_t nItems);

    public:
        // Consider making this private
        LineCursor::Ref lineCursor = nullptr;
    };
    class VerticalNavigationVSCode : public VerticalNavigationViewModel {
    public:
        VerticalNavigationVSCode() = default;
        virtual ~VerticalNavigationVSCode() = default;

        void OnNavigateDown(size_t rowsToMove, const Rect &viewRect, size_t nItems) override;
        void OnNavigateUp(size_t rowsToMove, const Rect &viewRect,  size_t nItems) override;
    };
    class VerticalNavigationCLion : public VerticalNavigationViewModel {
    public:
        VerticalNavigationCLion() = default;
        virtual ~VerticalNavigationCLion() = default;

        void OnNavigateDown(size_t rowsToMove, const Rect &viewRect, size_t nItems) override;
        void OnNavigateUp(size_t rowsToMove, const Rect &viewRect,  size_t nItems) override;

    };


}

#endif //EDITOR_VERTICALNAVIGATIONVIEWMODEL_H

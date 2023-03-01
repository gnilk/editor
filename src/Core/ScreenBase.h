//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_SCREENBASE_H
#define EDITOR_SCREENBASE_H

#include <utility>
#include <vector>
#include "Core/Rect.h"
#include "Core/WindowBase.h"

namespace gedit {
    class ScreenBase {
    public:
        ScreenBase() = default;
        virtual ~ScreenBase() = default;
        virtual bool Open() { return false; }
        virtual void Close() {}
        virtual void Clear() {}
        virtual void Update() {}
        virtual void BeginRefreshCycle() {}
        virtual void EndRefreshCycle() {}
        virtual Rect Dimensions() {
            return {};
        };
        virtual WindowBase *CreateWindow(const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) { return nullptr; }

    protected:
    };
}

#endif //EDITOR_SCREENBASE_H

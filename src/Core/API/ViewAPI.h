//
// Created by gnilk on 03.06.23.
//

#ifndef EDITOR_VIEWAPI_H
#define EDITOR_VIEWAPI_H

#include <memory>
#include "Core/Views/ViewBase.h"

namespace gedit {
    class ViewAPI {
    public:
        using Ref = std::shared_ptr<ViewAPI>;
    public:
        ViewAPI(ViewBase::Ref pView) : view(pView) {

        }
        virtual ~ViewAPI() = default;

        bool IsVisible();
        void SetVisible(bool bVisible);
    private:
        ViewBase::Ref view;
    };
}


#endif //EDITOR_VIEWAPI_H

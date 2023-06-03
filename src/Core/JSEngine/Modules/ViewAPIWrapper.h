//
// Created by gnilk on 03.06.23.
//

#ifndef EDITOR_VIEWAPIWRAPPER_H
#define EDITOR_VIEWAPIWRAPPER_H

#include <memory>
#include "duktape.h"
#include "Core/API/ViewAPI.h"

namespace gedit {
    class ViewAPIWrapper {
    public:
        using Ref = std::shared_ptr<ViewAPIWrapper>;
    public:
        ViewAPIWrapper() = default;
        ViewAPIWrapper(ViewAPI::Ref viewref) : view(viewref) {
        }

        static Ref Create(ViewAPI::Ref viewapi) {
            return std::make_shared<ViewAPIWrapper>(viewapi);
        }
        static void RegisterModule(duk_context *ctx);

        bool IsValid() {
            if (view != nullptr) {
                return true;
            }
            return false;
        }

        bool IsVisible() {
            return view->IsVisible();
        }
        void SetVisible(bool bVisible) {
            view->SetVisible(bVisible);
        }
    private:
        ViewAPI::Ref view;
    };
}


#endif //EDITOR_VIEWAPIWRAPPER_H

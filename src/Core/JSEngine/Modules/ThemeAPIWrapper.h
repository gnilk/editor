//
// Created by gnilk on 24.07.23.
//

#ifndef EDITOR_THEMEAPIWRAPPER_H
#define EDITOR_THEMEAPIWRAPPER_H

#include <memory>
#include "duktape.h"
#include "Core/API/ThemeAPI.h"

namespace gedit {
    class ThemeAPIWrapper {
    public:
        using Ref = std::shared_ptr<ThemeAPIWrapper>;

    public:
        ThemeAPIWrapper() = default;
        virtual ~ThemeAPIWrapper() = default;

        ThemeAPIWrapper(ThemeAPI::Ref refTheme) : theme(refTheme) {
        }

        static Ref Create(ThemeAPI::Ref refTheme) {
            return std::make_shared<ThemeAPIWrapper>(refTheme);
        }

        static void RegisterModule(duk_context *ctx);

        bool Reload();
    private:
        ThemeAPI::Ref theme = nullptr;

    };
}


#endif //EDITOR_THEMEAPIWRAPPER_H

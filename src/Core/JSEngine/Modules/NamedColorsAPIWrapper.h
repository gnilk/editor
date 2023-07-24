//
// Created by gnilk on 24.07.23.
//

#ifndef EDITOR_NAMEDCOLORSAPIWRAPPER_H
#define EDITOR_NAMEDCOLORSAPIWRAPPER_H

#include <memory>
#include "duktape.h"
#include "Core/API/NamedColorsAPI.h"
#include "Core/API/ThemeAPI.h"

namespace gedit {
    class NamedColorsAPIWrapper {
    public:
        using Ref = std::shared_ptr<NamedColorsAPIWrapper>;
    public:
        NamedColorsAPIWrapper() = default;
        virtual ~NamedColorsAPIWrapper() = default;

        NamedColorsAPIWrapper(NamedColorsAPI::Ref refColors) : colors(refColors) {
        }
        static Ref Create(NamedColorsAPI::Ref refColors) {
            return std::make_shared<NamedColorsAPIWrapper>(refColors);
        }

        static void RegisterModule(duk_context *ctx);

        std::vector<NamedColors::NamedColor::Ref> ToVector();

    private:
        NamedColorsAPI::Ref colors = nullptr;
    };
}


#endif //EDITOR_NAMEDCOLORSAPIWRAPPER_H

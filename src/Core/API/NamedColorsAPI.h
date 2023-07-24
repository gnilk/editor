//
// Created by gnilk on 24.07.23.
//

#ifndef EDITOR_NAMEDCOLORSAPI_H
#define EDITOR_NAMEDCOLORSAPI_H

#include <memory>
#include <vector>
#include "Core/NamedColors.h"

namespace gedit {
    class NamedColorsAPI {
    public:
        using Ref = std::shared_ptr<NamedColorsAPI>;
    public:
        NamedColorsAPI() = default;
        virtual ~NamedColorsAPI() = default;

        NamedColorsAPI(NamedColors::Ref refColors);

        const std::vector<NamedColors::NamedColor> &List();

    private:
        NamedColors::Ref colors = nullptr;
        std::vector<NamedColors::NamedColor> namedColors;

    };
}


#endif //EDITOR_NAMEDCOLORSAPI_H

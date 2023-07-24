//
// Created by gnilk on 24.07.23.
//

#include "Core/RuntimeConfig.h"
#include "NamedColorsAPI.h"

using namespace gedit;

NamedColorsAPI::NamedColorsAPI(NamedColors::Ref refColors) : colors(refColors) {

}

const std::vector<NamedColors::NamedColor> &NamedColorsAPI::List() {
    if (colors == nullptr) {
        return namedColors;
    }
    if (!namedColors.empty()) {
        namedColors.clear();
    }
    colors->ToVector(namedColors);
    return namedColors;
}
//
// Created by gnilk on 31.01.23.
//
#include "ColorConfig.h"

ColorConfig::ColorConfig() {
    SetDefaults();
}

void ColorConfig::SetDefaults() noexcept {

}

void ColorConfig::SetColor(const std::string &name, ColorRGBA color) {
    colors[name] = color;
}


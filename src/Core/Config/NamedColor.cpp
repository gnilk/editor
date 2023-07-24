//
// Created by gnilk on 31.01.23.
//
#include "NamedColor.h"

using namespace gedit;

NamedColor::NamedColor() {
    SetDefaults();
}

void NamedColor::SetDefaults() noexcept {

}

void NamedColor::SetColor(const std::string &name, ColorRGBA color) {
    colors[name] = color;
}


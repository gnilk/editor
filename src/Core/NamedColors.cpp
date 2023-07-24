//
// Created by gnilk on 31.01.23.
//
#include "NamedColors.h"

using namespace gedit;

NamedColors::NamedColors() {
    SetDefaults();
}

void NamedColors::SetDefaults() noexcept {

}

void NamedColors::SetColor(const std::string &name, ColorRGBA color) {
    colors[name] = color;
}


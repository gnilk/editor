//
// Created by gnilk on 31.01.23.
//
#include "NamedColorConfig.h"

using namespace gedit;

NamedColorConfig::NamedColorConfig() {
    SetDefaults();
}

void NamedColorConfig::SetDefaults() noexcept {

}

void NamedColorConfig::SetColor(const std::string &name, ColorRGBA color) {
    colors[name] = color;
}


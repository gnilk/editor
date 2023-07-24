//
// Created by gnilk on 31.01.23.
//
#include <vector>
#include <utility>
#include "NamedColors.h"

using namespace gedit;


void NamedColors::SetColor(const std::string &name, ColorRGBA color) {
    colors[name] = color;
}

void NamedColors::ToVector(std::vector<NamedColor> &flatVector) {
    for(auto &[name,color] : colors) {
        flatVector.push_back({name, color});
    }
}

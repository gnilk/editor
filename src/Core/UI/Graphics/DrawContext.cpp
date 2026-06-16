//
// Created by gnilk on 08.08.23.
//
#include "Core/UI/UIHost.h"
#include "DrawContext.h"

using namespace gedit;

void DrawContext::ResetDrawColors() const {
    auto &globalColors = UIHost::Instance().GetGlobalColors();

    auto newBgColor = globalColors.GetColor("background");
    auto newFgColor = globalColors.GetColor("foreground");
    SetColor(newFgColor, newBgColor);
}

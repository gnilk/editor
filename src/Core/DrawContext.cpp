//
// Created by gnilk on 08.08.23.
//
#include "Core/Editor.h"
#include "DrawContext.h"

using namespace gedit;

void DrawContext::ResetDrawColors() const {
    auto theme = Editor::Instance().GetTheme();

    auto newBgColor = theme->GetGlobalColors().GetColor("background");
    auto newFgColor = theme->GetGlobalColors().GetColor("foreground");
    SetColor(newFgColor, newBgColor);
}

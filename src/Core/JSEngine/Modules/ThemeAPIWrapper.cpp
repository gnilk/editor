//
// Created by gnilk on 24.07.23.
//
#include "dukglue/dukglue.h"

#include "Core/API/ThemeAPI.h"
#include "Core/Editor.h"
#include "ThemeAPIWrapper.h"

using namespace gedit;

void ThemeAPIWrapper::RegisterModule(duk_context *ctx) {
    dukglue_register_constructor_managed<ThemeAPIWrapper>(ctx, "Theme");
    dukglue_register_delete<ThemeAPIWrapper>(ctx);
    dukglue_register_method(ctx, &ThemeAPIWrapper::Reload, "Reload");
    dukglue_register_method(ctx, &ThemeAPIWrapper::GetColorsForClass, "GetColorsForClass");
}

bool ThemeAPIWrapper::Reload() {
    return theme->Reload();
}

NamedColorsAPIWrapper::Ref ThemeAPIWrapper::GetColorsForClass(const char *clsColors) {
    auto colors = theme->GetColors(clsColors);
    if (colors == nullptr) {
        return nullptr;
    }
    return NamedColorsAPIWrapper::Create(colors);
}
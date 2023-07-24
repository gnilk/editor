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
}

bool ThemeAPIWrapper::Reload() {
    return theme->Reload();
}
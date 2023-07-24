//
// Created by gnilk on 24.07.23.
//

#include "dukglue/dukglue.h"
#include "Core/NamedColors.h"
#include "Core/ColorRGBA.h"
#include "NamedColorsAPIWrapper.h"

using namespace gedit;

void NamedColorsAPIWrapper::RegisterModule(duk_context *ctx) {
    dukglue_register_constructor_managed<NamedColorsAPIWrapper>(ctx, "NamedColors");
    dukglue_register_delete<NamedColorsAPIWrapper>(ctx);
    dukglue_register_method(ctx, &NamedColorsAPIWrapper::ToVector, "ToVector");

    // We do this directly here...
    dukglue_register_constructor_managed<NamedColors::NamedColor>(ctx, "NamedColor");
    dukglue_register_delete<NamedColors::NamedColor>(ctx);
    dukglue_register_method(ctx, &NamedColors::NamedColor::GetName, "GetName");
    dukglue_register_method(ctx, &NamedColors::NamedColor::GetColor, "GetColor");

    dukglue_register_constructor_managed<ColorRGBA>(ctx, "ColorRGBA");
    dukglue_register_delete<ColorRGBA>(ctx);
    dukglue_register_property(ctx, &ColorRGBA::IntRed, nullptr, "r");
    dukglue_register_property(ctx, &ColorRGBA::IntGreen, nullptr, "g");
    dukglue_register_property(ctx, &ColorRGBA::IntBlue, nullptr, "b");
    dukglue_register_property(ctx, &ColorRGBA::IntAlpha, nullptr, "a");

}

std::vector<NamedColors::NamedColor::Ref> NamedColorsAPIWrapper::ToVector() {
    auto &colorList = colors->List();
    std::vector<NamedColors::NamedColor::Ref> colorNames;
    for(auto &[name, col] : colorList) {
        colorNames.push_back(NamedColors::NamedColor::Create(name,col));
    }
    std::sort(colorNames.begin(), colorNames.end(),[](NamedColors::NamedColor::Ref a, NamedColors::NamedColor::Ref b) -> bool{
        return b->GetName() > a->GetName();
    });
    return colorNames;
}

//
// Created by gnilk on 03.06.23.
//

#include "dukglue/dukglue.h"

#include "ViewAPIWrapper.h"

using namespace gedit;
void ViewAPIWrapper::RegisterModule(duk_context *ctx) {
    dukglue_register_constructor_managed<ViewAPIWrapper>(ctx, "View");
    dukglue_register_delete<ViewAPIWrapper>(ctx);
    dukglue_register_method(ctx, &ViewAPIWrapper::IsValid, "IsValid");
    dukglue_register_method(ctx, &ViewAPIWrapper::IsVisible, "IsVisible");
    dukglue_register_method(ctx, &ViewAPIWrapper::SetVisible, "SetVisible");
}

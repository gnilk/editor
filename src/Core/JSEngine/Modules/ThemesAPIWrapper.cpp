//
// See ThemesAPIWrapper.h
//

#include "dukglue/dukglue.h"

#include "Core/API/ThemesAPI.h"
#include "ThemesAPIWrapper.h"

using namespace gedit;

void ThemesAPIWrapper::RegisterModule(duk_context *ctx) {
    static ThemesAPIWrapper themesApiWrapper;

    dukglue_push(ctx, &themesApiWrapper);
    duk_put_global_string(ctx, "Themes");

    dukglue_register_method(ctx, &ThemesAPIWrapper::Import, "Import");
}

std::vector<std::string> ThemesAPIWrapper::Import(const char *file, const char *name, bool isForced) {
    ThemesAPI themesApi;
    return themesApi.Import(file, name, isForced).lines;
}

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
    dukglue_register_method(ctx, &ThemesAPIWrapper::List, "List");
    dukglue_register_method(ctx, &ThemesAPIWrapper::Use, "Use");
    dukglue_register_method(ctx, &ThemesAPIWrapper::Find, "Find");
    dukglue_register_method(ctx, &ThemesAPIWrapper::ProjectTheme, "ProjectTheme");
}

std::vector<std::string> ThemesAPIWrapper::Import(const char *file, const char *name, bool isForced) {
    ThemesAPI themesApi;
    return themesApi.Import(file, name, isForced).lines;
}

std::vector<std::string> ThemesAPIWrapper::List() {
    ThemesAPI themesApi;
    return themesApi.List().lines;
}

std::vector<std::string> ThemesAPIWrapper::Use(const char *name, const char *level) {
    auto configLevel = ConfigAPI::LevelFromString(level);
    if (!configLevel.has_value()) {
        return { std::string("Unknown level '") + level + "'" };
    }
    ThemesAPI themesApi;
    return themesApi.Use(name, configLevel.value()).lines;
}

std::string ThemesAPIWrapper::Find(const char *name) {
    return ThemesAPI::Find(name);
}

std::string ThemesAPIWrapper::ProjectTheme() {
    return ThemesAPI::ProjectTheme();
}

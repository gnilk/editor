//
// See ConfigAPIWrapper.h
//

#include "dukglue/dukglue.h"

#include "Core/API/ConfigAPI.h"
#include "Core/Editor.h"
#include "ConfigAPIWrapper.h"

using namespace gedit;

void ConfigAPIWrapper::RegisterModule(duk_context *ctx) {
    static ConfigAPIWrapper configApiWrapper;

    dukglue_push(ctx, &configApiWrapper);
    duk_put_global_string(ctx, "Config");

    dukglue_register_method(ctx, &ConfigAPIWrapper::List, "List");
    dukglue_register_method(ctx, &ConfigAPIWrapper::Get, "Get");
    dukglue_register_method(ctx, &ConfigAPIWrapper::Set, "Set");
    dukglue_register_method(ctx, &ConfigAPIWrapper::Edit, "Edit");
}

std::vector<std::string> ConfigAPIWrapper::List(const char *level, const char *prefix) {
    auto configLevel = ConfigAPI::LevelFromString(level);
    if (!configLevel.has_value()) {
        return { std::string("Unknown level '") + level + "'" };
    }
    auto configApi = Editor::Instance().GetGlobalAPIObject<ConfigAPI>();
    return configApi->List(configLevel.value(), prefix).lines;
}

std::vector<std::string> ConfigAPIWrapper::Get(const char *level, const char *key) {
    auto configLevel = ConfigAPI::LevelFromString(level);
    if (!configLevel.has_value()) {
        return { std::string("Unknown level '") + level + "'" };
    }
    auto configApi = Editor::Instance().GetGlobalAPIObject<ConfigAPI>();
    return configApi->Get(configLevel.value(), key).lines;
}

std::vector<std::string> ConfigAPIWrapper::Set(const char *level, const char *key, const char *value) {
    auto configLevel = ConfigAPI::LevelFromString(level);
    if (!configLevel.has_value()) {
        return { std::string("Unknown level '") + level + "'" };
    }
    auto configApi = Editor::Instance().GetGlobalAPIObject<ConfigAPI>();
    return configApi->Set(configLevel.value(), key, value).lines;
}

std::vector<std::string> ConfigAPIWrapper::Edit(const char *level) {
    auto configLevel = ConfigAPI::LevelFromString(level);
    if (!configLevel.has_value()) {
        return { std::string("Unknown level '") + level + "'" };
    }
    auto configApi = Editor::Instance().GetGlobalAPIObject<ConfigAPI>();
    return configApi->Edit(configLevel.value()).lines;
}

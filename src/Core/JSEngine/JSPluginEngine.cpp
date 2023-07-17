//
// Created by gnilk on 27.03.23.
//

#include <string>
#include <vector>
#include <filesystem>

#include "logger.h"

#include "Core/Config/Config.h"
#include "JSPluginEngine.h"
#include "Modules/TextBufferAPIWrapper.h"
#include "Modules/EditorAPIWrapper.h"
#include "Modules/ConsoleAPIWrapper.h"
#include "Modules/ViewAPIWrapper.h"
#include "Core/StrUtil.h"
#include "Core/RuntimeConfig.h"

#include "duk_module_node.h"

using namespace gedit;


// Forward declare - implemented at the end - helpers...
static void *LoadFile(const std::string &filename, size_t *outSz);
static duk_ret_t cb_resolve_module(duk_context *ctx);
static duk_ret_t cb_load_module(duk_context *ctx);


static void glbFatalErrorHandler(void *udata, const char *msg) {
    (void) udata;  /* ignored in this case, silence warning */

    /* Note that 'msg' may be NULL. */
    fprintf(stderr, "*** FATAL ERROR: %s\n", (msg ? msg : "no message"));
    fflush(stderr);
    abort();
}


bool JSPluginEngine::Initialize() {
    logger = gnilk::Logger::GetLogger("JSEngine");
    ctx = duk_create_heap(NULL, NULL, NULL, NULL, glbFatalErrorHandler);
    if (!ConfigureNodeModuleSupport()) {
        logger->Error("Failed to initalize node-module support loading");
        return false;
    }
    RegisterBuiltIns();

    ConfigNode config;
    auto pluginRoot = Config::Instance()["main"].GetStr("plugin_directory","");
    auto pluginConfigFile =  std::filesystem::path(pluginRoot).append("jsplugs.yml");

    if (!config.LoadConfig(pluginConfigFile.c_str())) {
        exit(1);
    }

    auto commands = config["commands"];
    auto ymlnode = commands.GetDataNode();
    for(auto node : ymlnode) {
        if (!node.IsMap()) {
            continue;
        }
        ConfigNode cfgPlugin(node);
        auto cmd = JSPluginCommand::CreateFromConfig(cfgPlugin);
        if (cmd == nullptr) {
            continue;
        }
        if (!cmd->TryLoad()) {
            continue;
        }
        cmd->SetExecutor(this);
        RuntimeConfig::Instance().RegisterPluginCommand(cmd);
        // All good, let's push it to our list of supported plugins...
        plugins.push_back(cmd);
    }
    return true;
}

bool JSPluginEngine::RunScriptOnce(const std::string_view script, const std::vector<std::string> &args) {
    return RunScriptOnce(std::string(script), args);
}


bool JSPluginEngine::RunScriptOnce(const std::string &script, const std::vector<std::string> &args) {
    bool result = true; // assume everything went fine..
    logger->Debug("Begin, stack is now: %d", (int)duk_get_top(ctx));

    // Consider creating a local context for running this function - this would (probably) remove the need to pop the function out of scope when done
    //
    // (void) duk_push_thread(ctx);
    // new_ctx = duk_get_context(ctx, -1 /*index*/);
    //


    // prepare and compile the script
    duk_push_string(ctx, script.c_str());
    duk_int_t idxEval = duk_peval(ctx);
    if (idxEval != 0) {
        logger->Error("%s", duk_safe_to_string(ctx,-1));
        return false;
    }
    duk_pop(ctx);
    duk_push_global_object(ctx);

    if (!duk_get_prop_string(ctx,-1,"main")) {
        duk_pop(ctx);
        duk_pop(ctx);
        logger->Error("You must begin your stuff with 'main'");
        return false;
    }

    // Call the function
    int argCounter = 0;
    auto argArrayIndex = duk_push_array(ctx);
    for(auto &s : args) {
        duk_push_string(ctx, s.c_str());
        duk_put_prop_index(ctx, argArrayIndex, argCounter);
        argCounter++;
    }

    if (duk_pcall(ctx, 1) != DUK_EXEC_SUCCESS) {
        logger->Error("%s", duk_safe_to_string(ctx, -1));
        result = false;
        // Still need to unwind stack and clean up the context - hence, no early exit in this case...
    } else {
        logger->Debug("Script Executed, Res=%d", duk_get_int(ctx, -1));
    }
    duk_pop(ctx);   // result
    if (!duk_del_prop_string(ctx,-1,"main")) {
        logger->Error("Unable to delete function");
        return false;
    }
    duk_pop(ctx);   // ctx (global)
    logger->Error("End, stack is now: %d", (int)duk_get_top(ctx));

    // Need to sweep twice - see documentation
    // First sweep marks, second sweep finializes..
    duk_gc(ctx,0);
    duk_gc(ctx,0);

    return result;
}


// TODO: Error checking...
bool JSPluginEngine::ConfigureNodeModuleSupport() {
    duk_push_object(ctx);
    duk_push_c_function(ctx, cb_resolve_module, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "resolve");
    duk_push_c_function(ctx, cb_load_module, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "load");
    duk_module_node_init(ctx);
    return true;
}

void JSPluginEngine::RegisterBuiltIns() {
    // Showing two ways of registering native objects (these are singletons)
//    fileio_register(ctx);
//    MyClass::RegisterModule(ctx);
//    //TextBufferAPIWrapper::RegisterModule(ctx);
//    EditorAPIWrapper::RegisterModule(ctx);
    EditorAPIWrapper::RegisterModule(ctx);
    TextBufferAPIWrapper::RegisterModule(ctx);
    ConsoleAPIWrapper::RegisterModule(ctx);
    ViewAPIWrapper::RegisterModule(ctx);
}


/// Helpers
// For node modules - this is lifted from the documentation - we should enhance/rewrite this
static duk_ret_t cb_resolve_module(duk_context *ctx) {
    /*
     *  Entry stack: [ requested_id parent_id ]
     */

//    const char *requested_id = duk_get_string(ctx, 0);
//    const char *parent_id = duk_get_string(ctx, 1);  /* calling module */
//    const char *resolved_id;
//
//    /* Arrive at the canonical module ID somehow. */
//
//    duk_push_string(ctx, resolved_id);

    // Just pass through of module id
    const char *module_id = duk_require_string(ctx, 0);
//    const char *parent_id = duk_require_string(ctx, 1);

    duk_push_sprintf(ctx, "%s.js", module_id);
//    printf("resolve_cb: id:'%s', parent-id:'%s', resolve-to:'%s'\n",
//           module_id, parent_id, duk_get_string(ctx, -1));

    return 1;   // Number of return values...
}

//
// Note: I know we must rewrite this properly
//
static duk_ret_t cb_load_module(duk_context *ctx) {
    /*
     *  Entry stack: [ resolved_id exports module ]
     */

    const char *filename;
    const char *module_id;

    module_id = duk_require_string(ctx, 0);
    duk_get_prop_string(ctx, 2, "filename");
    filename = duk_require_string(ctx, -1);


    std::string fullPath = "modules/";
    fullPath += filename;

//    printf("load_cb: id:'%s', filename:'%s'\n", module_id, fullPath.c_str()); //filename);

    size_t szBuffer;
    auto fileContents = LoadFile(fullPath, &szBuffer);
    if (fileContents == nullptr) {
        (void) duk_type_error(ctx, "cannot find module: %s (%s)", module_id, fullPath.c_str());
    }
    duk_push_string(ctx, (char *)fileContents);
    return 1;
}

static void *LoadFile(const std::string &filename, size_t *outSz) {

    auto path = std::filesystem::path(filename);
    if (!std::filesystem::exists(path)) {
        return nullptr;
    }
    auto szFile = std::filesystem::file_size(path);
    void *buffer = calloc(1,szFile + 1);


    auto f = fopen(path.c_str(), "r");
    fread(buffer, szFile, 1, f);
    fclose(f);
    *outSz = szFile;
    return buffer;
}





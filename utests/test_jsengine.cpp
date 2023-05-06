//
// Created by gnilk on 02.05.23.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/JSEngine/JSPluginEngine.h"
#include "Core/RuntimeConfig.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_jsengine(ITesting *t);
    DLL_EXPORT int test_jsengine_init(ITesting *t);
    DLL_EXPORT int test_jsengine_builtin(ITesting *t);
    DLL_EXPORT int test_jsengine_console(ITesting *t);
    DLL_EXPORT int test_jsengine_array(ITesting *t);
    DLL_EXPORT int test_jsengine_listlang(ITesting *t);
    DLL_EXPORT int test_jsengine_newbuffer(ITesting *t);
}

DLL_EXPORT int test_jsengine(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_jsengine_init(ITesting *t) {
    JSPluginEngine jsEngine;
    if (!jsEngine.Initialize()) {
        return kTR_Fail;
    }
    return kTR_Pass;
}

DLL_EXPORT int test_jsengine_builtin(ITesting *t) {
    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("setlanguage"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("setlanguage");
    TR_ASSERT(t, cmd != nullptr);
    cmd->Execute({".cpp"});
    return kTR_Pass;
}

DLL_EXPORT int test_jsengine_console(ITesting *t) {
    JSPluginEngine jsEngine;
    jsEngine.Initialize();

    std::string script = "function main(args) {"\
    "Console.WriteLine(\"Hello\");"\
    "}";
    jsEngine.RunScriptOnce(script, {});

    return kTR_Pass;
}
DLL_EXPORT int test_jsengine_array(ITesting *t) {
    JSPluginEngine jsEngine;
    jsEngine.Initialize();

    std::string script = "function main(args) {"\
    "Console.WriteLine(\"Before\");"\
    "var v = Editor.GetTestArray();" \
    "Console.WriteLine(\"After\");"\
    "Console.WriteLine(\"len: \", v.length);"\
    "for(i=0; i<v.length;i++) { Console.WriteLine(v[i]); }"\
    "}";
    jsEngine.RunScriptOnce(script, {});

    return kTR_Pass;
}
DLL_EXPORT int test_jsengine_listlang(ITesting *t) {
    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("listlanguages"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("listlanguages");
    TR_ASSERT(t, cmd != nullptr);
    cmd->Execute({});
    return kTR_Pass;
}

DLL_EXPORT int test_jsengine_newbuffer(ITesting *t) {
    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("newbuffer"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("newbuffer");
    TR_ASSERT(t, cmd != nullptr);
    auto numBefore = Editor::Instance().GetModels().size();
    cmd->Execute({"mamma"});
    auto numAfter = Editor::Instance().GetModels().size();
    TR_ASSERT(t, numAfter > numBefore);
    return kTR_Pass;
}

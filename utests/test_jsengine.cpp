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
    DLL_EXPORT int test_jsengine_newdocumentfromtext(ITesting *t);
    DLL_EXPORT int test_jsengine_loadbuffer(ITesting *t);
    DLL_EXPORT int test_jsengine_listbuffers(ITesting *t);
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
    auto numBefore = Editor::Instance().GetDocuments().size();
    cmd->Execute({"mamma"});
    auto numAfter = Editor::Instance().GetDocuments().size();
    TR_ASSERT(t, numAfter > numBefore);
    return kTR_Pass;
}

// End-to-end for the TS-2c JS seam: Editor.NewDocumentFromText (a 2-arg const char* method, dukglue-
// registered) must create a new document AND pre-fill its buffer from the '\n'-split text. This is the
// back half of the canonical "block to buffer" cmdlet (the front half, Console.GetSelectedBlock, is
// covered at the controller level by test_terminalcontroller_selected_block_text).
DLL_EXPORT int test_jsengine_newdocumentfromtext(ITesting *t) {
    JSPluginEngine jsEngine;
    jsEngine.Initialize();

    auto numBefore = Editor::Instance().GetDocuments().size();
    std::string script = "function main(args) {"\
    "Editor.NewDocumentFromText(\"blockout\", \"alpha\\nbeta\\ngamma\");"\
    "}";
    jsEngine.RunScriptOnce(script, {});

    TR_ASSERT(t, Editor::Instance().GetDocuments().size() > numBefore);

    auto doc = Editor::Instance().GetActiveDocument();
    TR_ASSERT(t, doc != nullptr);
    auto buffer = doc->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->NumLines() == 3);
    TR_ASSERT(t, buffer->LineAt(0)->Buffer() == std::u32string(U"alpha"));
    TR_ASSERT(t, buffer->LineAt(1)->Buffer() == std::u32string(U"beta"));
    TR_ASSERT(t, buffer->LineAt(2)->Buffer() == std::u32string(U"gamma"));

    return kTR_Pass;
}

DLL_EXPORT int test_jsengine_loadbuffer(ITesting *t) {
    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("openfile"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("openfile");
    TR_ASSERT(t, cmd != nullptr);
    // Don't assert on a raw count delta - some other test (or an earlier call to this same case, see
    // test_jsengine_listbuffers) may have already opened this fixture in the shared Editor singleton,
    // in which case re-opening it correctly reuses the existing Document (open-bugs.md #4) and the
    // count doesn't change. Assert the real invariant instead: the file ends up open.
    auto res = cmd->Execute({"testfiles/ConvertUTF.cpp"});
    auto isOpen = false;
    for (auto &document : Editor::Instance().GetDocuments()) {
        if (document->GetPath().filename() == "ConvertUTF.cpp") {
            isOpen = true;
            break;
        }
    }
    TR_ASSERT(t, isOpen);
    return kTR_Pass;
}

DLL_EXPORT int test_jsengine_listbuffers(ITesting *t) {
    // depends on loadbuffer having run first
    TR_ASSERT(t, test_jsengine_loadbuffer(t) == kTR_Pass);

    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("listbuffers"));
    auto cmd = RuntimeConfig::Instance().GetPluginCommand("listbuffers");
    TR_ASSERT(t, cmd != nullptr);
    cmd->Execute({});
    return kTR_Pass;
}

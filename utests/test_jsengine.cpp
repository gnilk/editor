//
// Created by gnilk on 02.05.23.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/JSEngine/JSPluginEngine.h"
#include "Core/RuntimeConfig.h"
#include "Core/Editor/Controllers/TerminalController.h"

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
    DLL_EXPORT int test_jsengine_terminalapi(ITesting *t);
    DLL_EXPORT int test_jsengine_copytoclipboard(ITesting *t);
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

// End-to-end for the TS-2c `Terminal` JS surface (the dedicated TerminalAPI/TerminalAPIWrapper). A real
// TerminalController is registered as the output console so the engine-agnostic TerminalAPI - which
// reads RuntimeConfig::OutputConsole() - resolves against it. We drive a deterministic CLOSED block
// (cmd1 -> alpha/beta, closed by committing cmd2), then a plugin script enumerates blocks by id and
// opens that block as a document. Asserts via the editor side-effect (the new doc's content), since JS
// return values aren't read back here.
DLL_EXPORT int test_jsengine_terminalapi(ITesting *t) {
    TerminalController controller;
    controller.Resize(20, 5);

    auto commit = [&](const std::u32string &cmd) {
        controller.GetInputLine()->Append(cmd);
        controller.CommitLine();
    };
    commit(U"cmd1");
    controller.WriteLine(U"alpha");
    controller.WriteLine(U"beta");
    commit(U"cmd2");   // closes cmd1's block -> [alpha, beta] is fixed

    auto *prevConsole = RuntimeConfig::Instance().OutputConsole();
    controller.RegisterAsOutputConsole();   // IOutputConsole is a private base; the controller upcasts

    JSPluginEngine jsEngine;
    jsEngine.Initialize();

    auto numBefore = Editor::Instance().GetDocuments().size();
    // blocks[0] = loose, blocks[1] = cmd1 (closed), blocks[2] = cmd2 (open tail).
    std::string script = "function main(args) {"\
    "var blocks = Terminal.GetBlocks();"\
    "if (blocks.length < 2) { return; }"\
    "Terminal.OpenBlockAsDocument(blocks[1].id);"\
    "}";
    jsEngine.RunScriptOnce(script, {});

    RuntimeConfig::Instance().SetOutputConsole(prevConsole);   // restore for the rest of the suite

    TR_ASSERT(t, Editor::Instance().GetDocuments().size() > numBefore);
    auto doc = Editor::Instance().GetActiveDocument();
    TR_ASSERT(t, doc != nullptr);
    auto buffer = doc->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->NumLines() == 2);
    TR_ASSERT(t, buffer->LineAt(0)->Buffer() == std::u32string(U"alpha"));
    TR_ASSERT(t, buffer->LineAt(1)->Buffer() == std::u32string(U"beta"));

    return kTR_Pass;
}

// The clipboard JS seam: Editor.CopyToClipboard(text) (the general primitive) and
// Terminal.CopyBlockToClipboard(id) (the block composite). Both land literal text on the paste buffer
// (RuntimeConfig clipboard top item, AsText round-tripping the text). The OnUpdate->OS-pasteboard
// wiring is covered at the clipboard level by test_clipboard_copytext_notifies.
DLL_EXPORT int test_jsengine_copytoclipboard(ITesting *t) {
    // Editor.CopyToClipboard primitive: arbitrary text lands on the paste buffer.
    {
        JSPluginEngine jsEngine;
        jsEngine.Initialize();
        std::string script = "function main(args) {"\
        "Editor.CopyToClipboard(\"hello\\nworld\");"\
        "}";
        jsEngine.RunScriptOnce(script, {});

        auto top = Editor::Instance().GetClipBoard().Top();
        TR_ASSERT(t, top != nullptr);
        TR_ASSERT(t, top->AsText() == std::u32string(U"hello\nworld"));
    }

    // Terminal.CopyBlockToClipboard composite: resolve a block by id -> paste buffer. Same controller-
    // as-console setup as test_jsengine_terminalapi; cmd1's block is closed (-> [alpha, beta]).
    {
        TerminalController controller;
        controller.Resize(20, 5);
        auto commit = [&](const std::u32string &cmd) {
            controller.GetInputLine()->Append(cmd);
            controller.CommitLine();
        };
        commit(U"cmd1");
        controller.WriteLine(U"alpha");
        controller.WriteLine(U"beta");
        commit(U"cmd2");   // closes cmd1's block -> [alpha, beta] is fixed

        auto *prevConsole = RuntimeConfig::Instance().OutputConsole();
        controller.RegisterAsOutputConsole();

        JSPluginEngine jsEngine;
        jsEngine.Initialize();
        std::string script = "function main(args) {"\
        "var blocks = Terminal.GetBlocks();"\
        "if (blocks.length < 2) { return; }"\
        "Terminal.CopyBlockToClipboard(blocks[1].id);"\
        "}";
        jsEngine.RunScriptOnce(script, {});

        RuntimeConfig::Instance().SetOutputConsole(prevConsole);

        auto top = Editor::Instance().GetClipBoard().Top();
        TR_ASSERT(t, top != nullptr);
        TR_ASSERT(t, top->AsText() == std::u32string(U"alpha\nbeta"));
    }

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

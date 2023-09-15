//
// Created by gnilk on 18.07.23.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/EditorModel.h"
#include "Core/Language/LanguageBase.h"
#include "Core/Language/LangLineTokenizer.h"
#include "Core/TextBuffer.h"


using namespace gedit;

extern "C" {
DLL_EXPORT int test_cpplang(ITesting *t);
DLL_EXPORT int test_cpplang_indent(ITesting *t);
DLL_EXPORT int test_cpplang_elseindent(ITesting *t);
}

DLL_EXPORT int test_cpplang(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_cpplang_indent(ITesting *t) {
    auto model = Editor::Instance().NewModel("test.cpp");
    auto buffer = model->GetTextBuffer();

    buffer->AddLineUTF8("if (a==b) {");
    buffer->AddLineUTF8("a");
    buffer->AddLineUTF8("b");
    buffer->AddLineUTF8("if (c==d) {");
    buffer->AddLineUTF8("c");
    buffer->AddLineUTF8("}");
    buffer->AddLineUTF8("}");

    buffer->Reparse();

    for(int i=0;i<buffer->NumLines();i++) {
        auto line = buffer->LineAt(i);
        printf("%d: indent: %d - data: %s\n", i, line->Indent(), line->BufferAsUTF8().c_str());
    }

    return kTR_Pass;
}

DLL_EXPORT int test_cpplang_elseindent(ITesting *t) {

    // Switch of threading for this...
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto model = Editor::Instance().NewModel("test.cpp");
    auto buffer = model->GetTextBuffer();

    buffer->AddLineUTF8("void func() {");
    buffer->AddLineUTF8("    if (a==b) {");
    buffer->AddLineUTF8("        ");
    buffer->AddLineUTF8("    } else {");
    buffer->AddLineUTF8("        ");
    buffer->AddLineUTF8("    }");
    buffer->AddLineUTF8("}");

    buffer->Reparse();

    static int correntIndent[]={0,0,1,2,1,2,1,0};
    for(int i=0;i<buffer->NumLines();i++) {
        auto line = buffer->LineAt(i);
        printf("%d: indent: %d - data: %s\n", i, line->Indent(), line->BufferAsUTF8().c_str());
        TR_ASSERT(t, line->Indent() == correntIndent[i]);
    }

    return kTR_Pass;

}

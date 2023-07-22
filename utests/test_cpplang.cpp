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

    buffer->AddLine("if (a==b) {");
    buffer->AddLine("a");
    buffer->AddLine("b");
    buffer->AddLine("if (c==d) {");
    buffer->AddLine("c");
    buffer->AddLine("}");
    buffer->AddLine("}");

    buffer->Reparse();

    for(int i=0;i<buffer->NumLines();i++) {
        auto line = buffer->LineAt(i);
        printf("%d: indent: %d - data: %s\n", i, line->Indent(), line->Buffer().data());
    }

    return kTR_Pass;
}

DLL_EXPORT int test_cpplang_elseindent(ITesting *t) {

    // Switch of threading for this...
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto model = Editor::Instance().NewModel("test.cpp");
    auto buffer = model->GetTextBuffer();

    buffer->AddLine("void func() {");
    buffer->AddLine("    if (a==b) {");
    buffer->AddLine("    } else {");
    buffer->AddLine("    }");
    buffer->AddLine("}");

    buffer->Reparse();

    for(int i=0;i<buffer->NumLines();i++) {
        auto line = buffer->LineAt(i);
        printf("%d: indent: %d - data: %s\n", i, line->Indent(), line->Buffer().data());
    }

    return kTR_Pass;

}

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
DLL_EXPORT int test_cpplang_include(ITesting *t);
DLL_EXPORT int test_cpplang_indent(ITesting *t);
DLL_EXPORT int test_cpplang_elseindent(ITesting *t);
DLL_EXPORT int test_cpplang_indentcode(ITesting *t);
DLL_EXPORT int test_cpplang_chardecl(ITesting *t);
DLL_EXPORT int test_cpplang_reparseregion(ITesting *t);
DLL_EXPORT int test_cpplang_keywords(ITesting *t);
}

DLL_EXPORT int test_cpplang(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    return kTR_Pass;
}
DLL_EXPORT int test_cpplang_include(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewModel("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);
    buffer->AddLineUTF8("#include \"test.h\";");
    buffer->AddLineUTF8("void main(int argc, char **argv) {");
    buffer->AddLineUTF8("  printf(\"hello world\");");
    buffer->AddLineUTF8("}");
    buffer->AddLineUTF8("");

    buffer->Reparse();

    for(int i=0;i<buffer->NumLines();i++) {
        auto line = buffer->LineAt(i);
    }


    return kTR_Pass;
}
DLL_EXPORT int test_cpplang_indent(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewModel("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

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
        printf("%d: indent: %d - data: %s\n", i, line->GetIndent(), line->BufferAsUTF8().c_str());
    }

    return kTR_Pass;
}

DLL_EXPORT int test_cpplang_elseindent(ITesting *t) {

    // Switch of threading for this...
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewModel("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

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
        printf("%d: indent: %d - data: %s\n", i, line->GetIndent(), line->BufferAsUTF8().c_str());
        TR_ASSERT(t, line->GetIndent() == correntIndent[i]);
    }

    TR_ASSERT(t, workspace->RemoveModel(model->GetModel()));

    return kTR_Pass;

}

DLL_EXPORT int test_cpplang_indentcode(ITesting *t) {
    // Switch of threading for this...
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto model = Editor::Instance().LoadModel("ConvertUTF.c");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->Reparse();
    for(int i=60;i<70;i++) {
        auto line = buffer->LineAt(i);
        printf("%d: indent: %d - data: %s\n", i, line->GetIndent(), line->BufferAsUTF8().c_str());
    }

    return kTR_Pass;

}


static void DumpLineData(const Line::Ref line) {
    auto ascii = UnicodeHelper::utf32toascii(line->Buffer());
    printf("Line: %s\n", ascii.c_str());
    for(auto &a : line->Attributes()) {
        printf("  %d, %d\n",a.idxOrigString, a.tokenClass);
        printf("  %s\n", ascii.c_str());
        for(int i=0;i<a.idxOrigString;i++) {
            printf(" ");
        }
        printf("  ^\n");
    }
}

DLL_EXPORT int test_cpplang_chardecl(ITesting *t) {
    // Switch of threading for this...
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewModel("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

//    buffer->AddLineUTF8("char *str=\"apa\"; // comment2");
    buffer->AddLineUTF8("char c='{'; // comment");
    buffer->Reparse();

    DumpLineData(buffer->LineAt(1));
    //DumpLineData(buffer->LineAt(2));

    TR_ASSERT(t, workspace->RemoveModel(model->GetModel()));

    return kTR_Pass;

}
DLL_EXPORT int test_cpplang_reparseregion(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewModel("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

//    buffer->AddLineUTF8("char *str=\"apa\"; // comment2");
    buffer->AddLineUTF8("/*");
    buffer->AddLineUTF8("this is in a comment");
    buffer->AddLineUTF8("*/");
    for(int i=0;i<10;i++) {
        buffer->AddLineUTF8("line;");
    }
    buffer->Reparse();
    printf("NLines before delete = %zu\n", buffer->NumLines());
    buffer->DeleteLineAt(3);
    printf("NLines after delete = %zu\n", buffer->NumLines());
    for(int i=0;i<4;i++) {
        auto l = buffer->LineAt(i);
        printf("%d: %s\n", i, UnicodeHelper::utf32toascii(l->Buffer()).c_str());
    }

    buffer->ReparseRegion(1,5);
    return kTR_Pass;
}

DLL_EXPORT int test_cpplang_keywords(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewModel("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

//    buffer->AddLineUTF8("char *str=\"apa\"; // comment2");
    buffer->AddLineUTF8("  ifelsevoidstatic");
    buffer->AddLineUTF8("if else void static");
    buffer->Reparse();

    struct Part {
        Line::LineAttrib attrib;
        std::u32string string;
    };


    std::vector<Part> parts;
    auto callback = [&parts](const Line::LineAttribIterator &itAttrib, std::u32string &strOut) {
        Part part;
        part.attrib = *itAttrib;
        part.string = strOut;
        parts.push_back(part);
    };

    auto line = buffer->LineAt(1);
    line->IterateWithAttributes(callback);
    TR_ASSERT(t, line->Attributes().size() == 2);
    TR_ASSERT(t, line->Attributes()[0].tokenClass == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, line->Attributes()[1].tokenClass == kLanguageTokenClass::kRegular);

    parts.clear();
    line = buffer->LineAt(2);
    TR_ASSERT(t, line->Attributes().size() == 4);
    TR_ASSERT(t, line->Attributes()[0].tokenClass == kLanguageTokenClass::kKeyword);    // if
    TR_ASSERT(t, line->Attributes()[1].tokenClass == kLanguageTokenClass::kKeyword);    // else
    TR_ASSERT(t, line->Attributes()[2].tokenClass == kLanguageTokenClass::kKnownType);  // void
    TR_ASSERT(t, line->Attributes()[3].tokenClass == kLanguageTokenClass::kKeyword);    // static
    line->IterateWithAttributes(callback);

    printf("ATTRIB: %zu\n", line->Attributes().size());

    return kTR_Pass;
}

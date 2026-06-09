//
// Created by gnilk on 18.07.23.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/Document.h"
#include "Core/Language/LanguageBase.h"
#include "Core/Language/LangLineTokenizer.h"
#include "Core/TextBuffer.h"


using namespace gedit;

extern "C" {
DLL_EXPORT int test_cpplang(ITesting *t);
DLL_EXPORT int test_cpplang_include(ITesting *t);
DLL_EXPORT int test_cpplang_ppmacro(ITesting *t);
DLL_EXPORT int test_cpplang_ppnoleak(ITesting *t);
DLL_EXPORT int test_cpplang_indent(ITesting *t);
DLL_EXPORT int test_cpplang_elseindent(ITesting *t);
DLL_EXPORT int test_cpplang_chardecl(ITesting *t);
DLL_EXPORT int test_cpplang_charop(ITesting *t);
DLL_EXPORT int test_cpplang_reparseregion(ITesting *t);
DLL_EXPORT int test_cpplang_reparseregion_bounds_normal(ITesting *t);
DLL_EXPORT int test_cpplang_reparseregion_bounds_blockcomment(ITesting *t);
DLL_EXPORT int test_cpplang_reparseregion_classification(ITesting *t);
DLL_EXPORT int test_cpplang_keywords(ITesting *t);
DLL_EXPORT int test_cpplang_types(ITesting *t);
DLL_EXPORT int test_cpplang_keywords_sample(ITesting *t);
}

DLL_EXPORT int test_cpplang(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    return kTR_Pass;
}


DLL_EXPORT int test_cpplang_include(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);
    buffer->AddLineUTF8("#include \"test.h\"");
    buffer->AddLineUTF8("#include <stdio.h>");
    buffer->AddLineUTF8("void main() {}");
    buffer->Reparse();

    for(int i=0;i<buffer->NumLines();i++) {
        auto line = buffer->LineAt(i);
        printf("%d: '%s'\n", i, line->BufferAsUTF8().c_str());
        for (auto &a : line->Attributes()) {
            printf("  idx=%d class=%d\n", a.idxOrigString, (int)a.tokenClass);
        }
    }

    // quoted form: '#' and 'include' are kPreProcessor, path content is kString
    auto quotedLine = buffer->LineAt(1);
    TR_ASSERT(t, quotedLine->Attributes()[0].tokenClass == kLanguageTokenClass::kPreProcessor);
    TR_ASSERT(t, quotedLine->Attributes()[1].tokenClass == kLanguageTokenClass::kPreProcessor);
    bool hasQuotedString = false;
    for (auto &a : quotedLine->Attributes()) {
        if (a.tokenClass == kLanguageTokenClass::kString) { hasQuotedString = true; }
    }
    TR_ASSERT(t, hasQuotedString);

    // angle-bracket form: '#' and 'include' are kPreProcessor, path content is kString
    auto angleLine = buffer->LineAt(2);
    TR_ASSERT(t, angleLine->Attributes()[0].tokenClass == kLanguageTokenClass::kPreProcessor);
    TR_ASSERT(t, angleLine->Attributes()[1].tokenClass == kLanguageTokenClass::kPreProcessor);
    bool hasAngleString = false;
    for (auto &a : angleLine->Attributes()) {
        if (a.tokenClass == kLanguageTokenClass::kString) { hasAngleString = true; }
    }
    TR_ASSERT(t, hasAngleString);

    TR_ASSERT(t, workspace->RemoveDocument(model->GetDocument()));
    return kTR_Pass;
}

// Helper: does the line contain a token of the given class?
static bool lineHasClass(const Line::Ref &line, kLanguageTokenClass cls) {
    for (auto &a : line->Attributes()) {
        if (a.tokenClass == cls) { return true; }
    }
    return false;
}

DLL_EXPORT int test_cpplang_ppmacro(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);
    buffer->AddLineUTF8("#ifdef FOO");
    buffer->AddLineUTF8("#ifndef BAR_H");
    buffer->AddLineUTF8("#undef BAZ");
    buffer->AddLineUTF8("int x = 0;");
    buffer->Reparse();

    for(int i=0;i<buffer->NumLines();i++) {
        auto line = buffer->LineAt(i);
        printf("%d: '%s'\n", i, line->BufferAsUTF8().c_str());
        for (auto &a : line->Attributes()) {
            printf("  idx=%d class=%d\n", a.idxOrigString, (int)a.tokenClass);
        }
    }

    // Each conditional: '#' and the directive keyword are kPreProcessor, the macro name is kMacroIdentifier
    for (int i = 1; i <= 3; i++) {
        auto line = buffer->LineAt(i);
        TR_ASSERT(t, line->Attributes()[0].tokenClass == kLanguageTokenClass::kPreProcessor);
        TR_ASSERT(t, line->Attributes()[1].tokenClass == kLanguageTokenClass::kPreProcessor);
        TR_ASSERT(t, lineHasClass(line, kLanguageTokenClass::kMacroIdentifier));
    }

    // The plain code line that follows must tokenize normally - confirms the directive states
    // (in_preprocessor + in_pp_macro) fully unwound at EOL and didn't leak.
    auto codeLine = buffer->LineAt(4);
    TR_ASSERT(t, !lineHasClass(codeLine, kLanguageTokenClass::kPreProcessor));
    TR_ASSERT(t, !lineHasClass(codeLine, kLanguageTokenClass::kMacroIdentifier));

    TR_ASSERT(t, workspace->RemoveDocument(model->GetDocument()));
    return kTR_Pass;
}

// Regression: a directive that stays in in_preprocessor and contains an operator/quote
// (e.g. #pragma ... "string") must not leak the preprocessor state onto following lines.
DLL_EXPORT int test_cpplang_ppnoleak(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);
    buffer->AddLineUTF8("#pragma GCC diagnostic push");
    buffer->AddLineUTF8("#pragma GCC diagnostic ignored \"-Wimplicit-fallthrough\"");
    buffer->AddLineUTF8("#define UNI_SUR_HIGH_START  (UTF32)0xD800");
    buffer->AddLineUTF8("int x = 0;");
    buffer->Reparse();

    for(int i=0;i<buffer->NumLines();i++) {
        auto line = buffer->LineAt(i);
        printf("%d: '%s'\n", i, line->BufferAsUTF8().c_str());
        for (auto &a : line->Attributes()) {
            printf("  idx=%d class=%d\n", a.idxOrigString, (int)a.tokenClass);
        }
    }

    // The lines after the quoted #pragma must tokenize on their own merits, not inherit the
    // preprocessor state. The plain code line is the clearest tell of a leak.
    auto codeLine = buffer->LineAt(4);
    TR_ASSERT(t, codeLine->Attributes()[0].tokenClass == kLanguageTokenClass::kKnownType); // 'int'
    TR_ASSERT(t, !lineHasClass(codeLine, kLanguageTokenClass::kPreProcessor));
    TR_ASSERT(t, !lineHasClass(codeLine, kLanguageTokenClass::kMacroIdentifier));

    // And #define on line 3 must still start as a fresh preprocessor directive
    auto defineLine = buffer->LineAt(3);
    TR_ASSERT(t, defineLine->Attributes()[0].tokenClass == kLanguageTokenClass::kPreProcessor); // '#'

    TR_ASSERT(t, workspace->RemoveDocument(model->GetDocument()));
    return kTR_Pass;
}

DLL_EXPORT int test_cpplang_indent(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.cpp");
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

    static const int expectedIndent[] = {0, 0, 1, 1, 1, 2, 1, 0};
    for(int i=0;i<buffer->NumLines();i++) {
        auto line = buffer->LineAt(i);
        printf("%d: indent: %d - data: %s\n", i, line->GetIndent(), line->BufferAsUTF8().c_str());
        TR_ASSERT(t, line->GetIndent() == expectedIndent[i]);
    }

    TR_ASSERT(t, workspace->RemoveDocument(model->GetDocument()));

    return kTR_Pass;
}

DLL_EXPORT int test_cpplang_elseindent(ITesting *t) {

    // Switch of threading for this...
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.cpp");
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

    TR_ASSERT(t, workspace->RemoveDocument(model->GetDocument()));

    return kTR_Pass;

}



static void DumpLineData(const Line::Ref line) {
    auto ascii = UnicodeHelper::utf32toascii(line->Buffer());
    printf("Line: %s\n", ascii.c_str());
    for(auto &a : line->Attributes()) {
        printf("  %d, %d (%s)\n",a.idxOrigString, a.tokenClass, LanguageTokenClassToString(a.tokenClass).c_str());
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
    auto model = workspace->NewDocument("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

//    buffer->AddLineUTF8("char *str=\"apa\"; // comment2");
    buffer->AddLineUTF8("char c='{'; // comment");
    buffer->Reparse();

    DumpLineData(buffer->LineAt(1));
    //DumpLineData(buffer->LineAt(2));

    TR_ASSERT(t, workspace->RemoveDocument(model->GetDocument()));

    return kTR_Pass;
}

DLL_EXPORT int test_cpplang_charop(ITesting *t) {
    // Switch of threading for this...
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);


    // Does this work???
    //std::string str = R"(char q = ((1==2)?'"':'\'');   /* Quote character */)";
    std::string str = R"('\'';   /* Quote character */)";
    //buffer->AddLine(UR"_("char q = ((1==2)?'"':'\'');   /* Quote character */")_");
    //auto u32instrOp = UnicodeHelper::utf8to32(str);
    buffer->AddLineUTF8(str.c_str());

    buffer->Reparse();

    auto line = buffer->LineAt(1);
    TR_ASSERT(t, line->Attributes().size() > 2);
    auto last = *(line->Attributes().end()-1);
    TR_ASSERT(t, last.tokenClass == kLanguageTokenClass::kBlockComment);

    printf("STR: %s\n",line->BufferAsUTF8().c_str());

    return kTR_Pass;
}

DLL_EXPORT int test_cpplang_reparseregion(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);
    return kTR_Pass;
}

// Verify ComputeParseRegion doesn't extend bounds for plain code (all lines at depth 1)
DLL_EXPORT int test_cpplang_reparseregion_bounds_normal(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    for (int i = 0; i < 10; i++) {
        buffer->AddLineUTF8("line;");
    }
    buffer->Reparse();

    auto &tokenizer = buffer->GetLanguage().Tokenizer();
    const auto &lines = buffer->Lines();

    // Editing at line 6 — no block constructs, so region is just one line in each direction
    auto [start, end] = tokenizer.ComputeParseRegion(lines, 6, 6);
    printf("normal bounds: start=%zu end=%zu\n", start, end);
    TR_ASSERT(t, start == 5);
    TR_ASSERT(t, end == 7);

    TR_ASSERT(t, workspace->RemoveDocument(model->GetDocument()));
    return kTR_Pass;
}

// Verify ComputeParseRegion extends back/forward to cover an enclosing block comment
DLL_EXPORT int test_cpplang_reparseregion_bounds_blockcomment(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("line;");       // idx 1
    buffer->AddLineUTF8("line;");       // idx 2
    buffer->AddLineUTF8("line;");       // idx 3
    buffer->AddLineUTF8("/*");          // idx 4 — depth 1 at start, 2 after
    buffer->AddLineUTF8("comment 1");   // idx 5 — depth 2
    buffer->AddLineUTF8("comment 2");   // idx 6 — depth 2
    buffer->AddLineUTF8("*/");          // idx 7 — depth 2 at start, 1 after
    buffer->AddLineUTF8("line;");       // idx 8
    buffer->AddLineUTF8("line;");       // idx 9
    buffer->AddLineUTF8("line;");       // idx 10
    buffer->Reparse();

    auto &tokenizer = buffer->GetLanguage().Tokenizer();
    const auto &lines = buffer->Lines();

    // Print state stack depths so we can see the parse state
    for (int i = 0; i < (int)lines.size(); i++) {
        printf("  line %d depth=%d  '%s'\n", i, lines[i]->GetStateStackDepth(),
               UnicodeHelper::utf32toascii(lines[i]->Buffer()).c_str());
    }

    // Editing line 6 (inside comment): region must extend back to line 4 (/* line)
    // and forward past line 7 (*/) to line 8
    auto [start, end] = tokenizer.ComputeParseRegion(lines, 6, 6);
    printf("blockcomment bounds: start=%zu end=%zu\n", start, end);
    TR_ASSERT(t, start == 4);
    TR_ASSERT(t, end == 8);

    TR_ASSERT(t, workspace->RemoveDocument(model->GetDocument()));
    return kTR_Pass;
}

// Verify token classifications after parsing a block comment
DLL_EXPORT int test_cpplang_reparseregion_classification(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("/*");          // idx 1
    buffer->AddLineUTF8("comment");     // idx 2
    buffer->AddLineUTF8("*/");          // idx 3
    buffer->AddLineUTF8("line;");       // idx 4
    buffer->Reparse();

    for (int i = 1; i <= 4; i++) {
        DumpLineData(buffer->LineAt(i));
    }

    auto openLine = buffer->LineAt(1);
    TR_ASSERT(t, openLine->Attributes().size() > 0);
    TR_ASSERT(t, openLine->Attributes()[0].tokenClass == kLanguageTokenClass::kBlockComment);

    auto commentLine = buffer->LineAt(2);
    TR_ASSERT(t, commentLine->Attributes().size() > 0);
    TR_ASSERT(t, commentLine->Attributes()[0].tokenClass == kLanguageTokenClass::kCommentedText);

    auto closeLine = buffer->LineAt(3);
    TR_ASSERT(t, closeLine->Attributes().size() > 0);
    TR_ASSERT(t, closeLine->Attributes()[0].tokenClass == kLanguageTokenClass::kBlockComment);

    auto normalLine = buffer->LineAt(4);
    TR_ASSERT(t, normalLine->Attributes().size() > 0);
    TR_ASSERT(t, normalLine->Attributes()[0].tokenClass == kLanguageTokenClass::kRegular);

    TR_ASSERT(t, workspace->RemoveDocument(model->GetDocument()));
    return kTR_Pass;
}

DLL_EXPORT int test_cpplang_keywords(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.cpp");
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

// Verify a representative sample of the extended cppTypes list is classified as kKnownType
DLL_EXPORT int test_cpplang_types(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    // original types
    buffer->AddLineUTF8("void char int float double");
    // bool (was missing)
    buffer->AddLineUTF8("bool");
    // fixed-width integers
    buffer->AddLineUTF8("int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t");
    // size/pointer types
    buffer->AddLineUTF8("size_t ssize_t ptrdiff_t intptr_t uintptr_t");
    // fixed-width floats
    buffer->AddLineUTF8("float16_t float32_t float64_t float128_t bfloat16_t");
    buffer->Reparse();

    auto checkAllKnownType = [&](int lineIdx) {
        auto line = buffer->LineAt(lineIdx);
        for (auto &a : line->Attributes()) {
            TR_ASSERT(t, a.tokenClass == kLanguageTokenClass::kKnownType);
        }
    };

    checkAllKnownType(1);   // void char int float double
    checkAllKnownType(2);   // bool
    checkAllKnownType(3);   // fixed-width integers
    checkAllKnownType(4);   // size/pointer types
    checkAllKnownType(5);   // fixed-width floats

    TR_ASSERT(t, workspace->RemoveDocument(model->GetDocument()));
    return kTR_Pass;
}

// Spot-check a spread of keywords across the list, including C++20 additions
DLL_EXPORT int test_cpplang_keywords_sample(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    // consteval (C++20, recently fixed typo), co_yield (coroutines),
    // requires (concepts), thread_local, reinterpret_cast
    buffer->AddLineUTF8("consteval co_yield requires thread_local reinterpret_cast");
    buffer->Reparse();

    auto line = buffer->LineAt(1);
    printf("keywords_sample attribs: %zu\n", line->Attributes().size());
    for (auto &a : line->Attributes()) {
        printf("  idx=%d class=%d\n", a.idxOrigString, (int)a.tokenClass);
    }

    TR_ASSERT(t, line->Attributes().size() == 5);
    TR_ASSERT(t, line->Attributes()[0].tokenClass == kLanguageTokenClass::kKeyword);  // consteval
    TR_ASSERT(t, line->Attributes()[1].tokenClass == kLanguageTokenClass::kKeyword);  // co_yield
    TR_ASSERT(t, line->Attributes()[2].tokenClass == kLanguageTokenClass::kKeyword);  // requires
    TR_ASSERT(t, line->Attributes()[3].tokenClass == kLanguageTokenClass::kKeyword);  // thread_local
    TR_ASSERT(t, line->Attributes()[4].tokenClass == kLanguageTokenClass::kKeyword);  // reinterpret_cast

    TR_ASSERT(t, workspace->RemoveDocument(model->GetDocument()));
    return kTR_Pass;
}

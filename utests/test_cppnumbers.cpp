//
// Created by gnilk on 02.06.2026.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/EditorModel.h"
#include "Core/TextBuffer.h"
#include "Core/Language/LanguageSupport/CPPNumberMatcher.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_cppnumbers(ITesting *t);
DLL_EXPORT int test_cppnumbers_decimal(ITesting *t);
DLL_EXPORT int test_cppnumbers_float(ITesting *t);
DLL_EXPORT int test_cppnumbers_hexbin(ITesting *t);
DLL_EXPORT int test_cppnumbers_notanumber(ITesting *t);
DLL_EXPORT int test_cppnumbers_integration(ITesting *t);
}

// Helper: run the matcher on a literal and return chars consumed
static int M(const std::u32string &s) {
    CPPNumberMatcher matcher;
    return matcher.Match(s);
}

DLL_EXPORT int test_cppnumbers(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_cppnumbers_decimal(ITesting *t) {
    TR_ASSERT(t, M(U"0") == 1);
    TR_ASSERT(t, M(U"123") == 3);
    TR_ASSERT(t, M(U"1'000") == 5);          // C++14 digit separator
    TR_ASSERT(t, M(U"123u") == 4);           // unsigned suffix
    TR_ASSERT(t, M(U"123UL") == 5);          // unsigned long suffix
    TR_ASSERT(t, M(U"42;") == 2);            // stops at terminator
    TR_ASSERT(t, M(U"7+3") == 1);           // stops at operator
    return kTR_Pass;
}

DLL_EXPORT int test_cppnumbers_float(ITesting *t) {
    TR_ASSERT(t, M(U"3.14") == 4);
    TR_ASSERT(t, M(U".5") == 2);             // leading-dot float
    TR_ASSERT(t, M(U"1.0f") == 4);           // float suffix
    TR_ASSERT(t, M(U"1.5e-3") == 6);         // signed exponent
    TR_ASSERT(t, M(U"2E10") == 4);           // bare exponent
    TR_ASSERT(t, M(U"1.5e-3f") == 7);        // exponent + suffix
    return kTR_Pass;
}

DLL_EXPORT int test_cppnumbers_hexbin(ITesting *t) {
    TR_ASSERT(t, M(U"0xFF") == 4);
    TR_ASSERT(t, M(U"0Xabc") == 5);
    TR_ASSERT(t, M(U"0xFFu") == 5);          // hex + suffix
    TR_ASSERT(t, M(U"0b1010") == 6);
    TR_ASSERT(t, M(U"0B0101") == 6);
    TR_ASSERT(t, M(U"0xDE'AD") == 7);        // hex with separator
    return kTR_Pass;
}

DLL_EXPORT int test_cppnumbers_notanumber(ITesting *t) {
    TR_ASSERT(t, M(U"") == 0);
    TR_ASSERT(t, M(U"abc") == 0);
    TR_ASSERT(t, M(U"hello") == 0);
    TR_ASSERT(t, M(U".x") == 0);             // dot not followed by digit
    TR_ASSERT(t, M(U"0x") == 0);             // prefix with no digits
    TR_ASSERT(t, M(U"_123") == 0);           // identifiers don't start with a number here
    return kTR_Pass;
}

// Verify numbers are classified as kNumber when parsed through the tokenizer
DLL_EXPORT int test_cppnumbers_integration(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewModel("test.cpp");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("int x = 42;");
    buffer->AddLineUTF8("float y = 3.14f;");
    buffer->Reparse();

    for (int i = 1; i <= 2; i++) {
        auto line = buffer->LineAt(i);
        printf("%d: '%s'\n", i, line->BufferAsUTF8().c_str());
        for (auto &a : line->Attributes()) {
            printf("  idx=%d class=%d\n", a.idxOrigString, (int)a.tokenClass);
        }
    }

    auto hasNumber = [](const Line::Ref &line) {
        for (auto &a : line->Attributes()) {
            if (a.tokenClass == kLanguageTokenClass::kNumber) { return true; }
        }
        return false;
    };

    TR_ASSERT(t, hasNumber(buffer->LineAt(1)));   // 42
    TR_ASSERT(t, hasNumber(buffer->LineAt(2)));   // 3.14f

    TR_ASSERT(t, workspace->RemoveModel(model->GetModel()));
    return kTR_Pass;
}

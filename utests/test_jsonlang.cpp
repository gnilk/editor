//
// Created by gnilk on 01.06.2026.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/Document.h"
#include "Core/Language/LanguageBase.h"
#include "Core/Language/LangLineTokenizer.h"
#include "Core/TextBuffer.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_jsonlang(ITesting *t);
DLL_EXPORT int test_jsonlang_string(ITesting *t);
DLL_EXPORT int test_jsonlang_keywords(ITesting *t);
DLL_EXPORT int test_jsonlang_indent(ITesting *t);
}

DLL_EXPORT int test_jsonlang(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);
    return kTR_Pass;
}

// Verify string content is classified as kString
DLL_EXPORT int test_jsonlang_string(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.json");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("\"hello\"");
    buffer->Reparse();

    auto line = buffer->LineAt(1);
    printf("json_string attribs: %zu\n", line->Attributes().size());
    for (auto &a : line->Attributes()) {
        printf("  idx=%d class=%d\n", a.idxOrigString, (int)a.tokenClass);
    }

    // Content between quotes should be kString
    bool hasString = false;
    for (auto &a : line->Attributes()) {
        if (a.tokenClass == kLanguageTokenClass::kString) {
            hasString = true;
        }
    }
    TR_ASSERT(t, hasString);

    TR_ASSERT(t, workspace->RemoveDocument(model->GetDocument()));
    return kTR_Pass;
}

// Verify JSON keywords (true, false, null) are classified as kKeyword
DLL_EXPORT int test_jsonlang_keywords(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.json");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("true false null");
    buffer->Reparse();

    auto line = buffer->LineAt(1);
    printf("json_keywords attribs: %zu\n", line->Attributes().size());
    for (auto &a : line->Attributes()) {
        printf("  idx=%d class=%d\n", a.idxOrigString, (int)a.tokenClass);
    }

    TR_ASSERT(t, line->Attributes().size() == 3);
    TR_ASSERT(t, line->Attributes()[0].tokenClass == kLanguageTokenClass::kKeyword);  // true
    TR_ASSERT(t, line->Attributes()[1].tokenClass == kLanguageTokenClass::kKeyword);  // false
    TR_ASSERT(t, line->Attributes()[2].tokenClass == kLanguageTokenClass::kKeyword);  // null

    TR_ASSERT(t, workspace->RemoveDocument(model->GetDocument()));
    return kTR_Pass;
}

// Verify { } and [ ] drive indentation correctly
DLL_EXPORT int test_jsonlang_indent(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto model = workspace->NewDocument("test.json");
    TR_ASSERT(t, model != nullptr);
    auto buffer = model->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("{");          // idx 1 — indent 0, next 1
    buffer->AddLineUTF8("  \"key\": ["); // idx 2 — indent 1, next 2
    buffer->AddLineUTF8("    1");       // idx 3 — indent 2
    buffer->AddLineUTF8("  ]");         // idx 4 — indent 1
    buffer->AddLineUTF8("}");           // idx 5 — indent 0
    buffer->Reparse();

    for (int i = 0; i < (int)buffer->NumLines(); i++) {
        auto line = buffer->LineAt(i);
        printf("%d: indent=%d  '%s'\n", i, line->GetIndent(), line->BufferAsUTF8().c_str());
    }

    static const int expectedIndent[] = {0, 0, 1, 2, 1, 0};
    for (int i = 0; i < (int)buffer->NumLines(); i++) {
        TR_ASSERT(t, buffer->LineAt(i)->GetIndent() == expectedIndent[i]);
    }

    TR_ASSERT(t, workspace->RemoveDocument(model->GetDocument()));
    return kTR_Pass;
}

//
// Created by gnilk on 15.06.26.
//
// Markdown language tests (§2.A inline + fenced spans). Asserts the classification PROPERTY, not
// magic attribute counts where avoidable. See docs/support-markdown.md.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/Document.h"
#include "Core/Language/LanguageBase.h"
#include "Core/Language/LangLineTokenizer.h"
#include "Core/TextBuffer.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_markdown(ITesting *t);
DLL_EXPORT int test_markdown_inlinecode(ITesting *t);
DLL_EXPORT int test_markdown_strong(ITesting *t);
DLL_EXPORT int test_markdown_emphasis(ITesting *t);
DLL_EXPORT int test_markdown_fence_spans_lines(ITesting *t);
}

// Does any attribute on the line carry the given class?
static bool lineHasClass(const Line::Ref &line, kLanguageTokenClass cls) {
    for (auto &a : line->Attributes()) {
        if (a.tokenClass == cls) {
            return true;
        }
    }
    return false;
}

DLL_EXPORT int test_markdown(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);
    return kTR_Pass;
}

// Inline `code` is classified kCode
DLL_EXPORT int test_markdown_inlinecode(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto node = workspace->NewDocument("test.md");
    TR_ASSERT(t, node != nullptr);
    auto buffer = node->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("a `code` b");
    buffer->Reparse();

    auto line = buffer->LineAt(1);
    TR_ASSERT(t, lineHasClass(line, kLanguageTokenClass::kCode));

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    return kTR_Pass;
}

// **bold** is classified kStrong (and not kEmphasis)
DLL_EXPORT int test_markdown_strong(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    auto node = workspace->NewDocument("test.md");
    auto buffer = node->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("a **bold** b");
    buffer->Reparse();

    auto line = buffer->LineAt(1);
    TR_ASSERT(t, lineHasClass(line, kLanguageTokenClass::kStrong));

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    return kTR_Pass;
}

// *italic* is classified kEmphasis
DLL_EXPORT int test_markdown_emphasis(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    auto node = workspace->NewDocument("test.md");
    auto buffer = node->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("a *italic* b");
    buffer->Reparse();

    auto line = buffer->LineAt(1);
    TR_ASSERT(t, lineHasClass(line, kLanguageTokenClass::kEmphasis));

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    return kTR_Pass;
}

// The key cross-line property: a fenced block persists across lines, so content between the fences is
// kCode and is NOT reinterpreted (a '*' inside the fence must NOT become emphasis).
DLL_EXPORT int test_markdown_fence_spans_lines(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    auto node = workspace->NewDocument("test.md");
    auto buffer = node->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("```");
    buffer->AddLineUTF8("x = a * b");
    buffer->AddLineUTF8("```");
    buffer->Reparse();

    auto inside = buffer->LineAt(2);
    TR_ASSERT(t, lineHasClass(inside, kLanguageTokenClass::kCode));
    // '*' inside the fence is verbatim code, not emphasis
    TR_ASSERT(t, !lineHasClass(inside, kLanguageTokenClass::kEmphasis));

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    return kTR_Pass;
}

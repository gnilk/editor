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
DLL_EXPORT int test_markdown_heading(ITesting *t);
DLL_EXPORT int test_markdown_heading_not_midline(ITesting *t);
DLL_EXPORT int test_markdown_blockquote(ITesting *t);
DLL_EXPORT int test_markdown_listmarker(ITesting *t);
DLL_EXPORT int test_markdown_listmarker_ordered(ITesting *t);
DLL_EXPORT int test_markdown_rule(ITesting *t);
DLL_EXPORT int test_markdown_heading_in_fence_is_code(ITesting *t);
DLL_EXPORT int test_markdown_fence_closes(ITesting *t);
DLL_EXPORT int test_markdown_strong_not_emphasis(ITesting *t);
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

// ATX heading: '# Title' -> the line carries kHeading (§2.B post-process)
DLL_EXPORT int test_markdown_heading(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    auto node = workspace->NewDocument("test.md");
    auto buffer = node->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("## A heading");
    buffer->Reparse();

    auto line = buffer->LineAt(1);
    TR_ASSERT(t, lineHasClass(line, kLanguageTokenClass::kHeading));

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    return kTR_Pass;
}

// A '#' mid-sentence is NOT a heading (line-anchored only)
DLL_EXPORT int test_markdown_heading_not_midline(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    auto node = workspace->NewDocument("test.md");
    auto buffer = node->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("issue #42 is open");
    buffer->Reparse();

    auto line = buffer->LineAt(1);
    TR_ASSERT(t, !lineHasClass(line, kLanguageTokenClass::kHeading));

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    return kTR_Pass;
}

// Blockquote: '>' at line start -> kBlockQuote
DLL_EXPORT int test_markdown_blockquote(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    auto node = workspace->NewDocument("test.md");
    auto buffer = node->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("> quoted text");
    buffer->Reparse();

    auto line = buffer->LineAt(1);
    TR_ASSERT(t, lineHasClass(line, kLanguageTokenClass::kBlockQuote));

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    return kTR_Pass;
}

// Unordered list marker '- ' -> the marker is kListMarker; item text is NOT swallowed as the marker
// class (the dash marker only colors one char).
DLL_EXPORT int test_markdown_listmarker(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    auto node = workspace->NewDocument("test.md");
    auto buffer = node->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("- an item");
    buffer->Reparse();

    auto line = buffer->LineAt(1);
    TR_ASSERT(t, lineHasClass(line, kLanguageTokenClass::kListMarker));
    // marker should be a single char span: a regular span follows it
    TR_ASSERT(t, lineHasClass(line, kLanguageTokenClass::kRegular));

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    return kTR_Pass;
}

// Ordered list marker '1. ' -> kListMarker (the digit-run + '.'/'.)' path in §2.B)
DLL_EXPORT int test_markdown_listmarker_ordered(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    auto node = workspace->NewDocument("test.md");
    auto buffer = node->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("1. first item");
    buffer->Reparse();

    auto line = buffer->LineAt(1);
    TR_ASSERT(t, lineHasClass(line, kLanguageTokenClass::kListMarker));
    // item text after the marker is not swallowed as the marker class
    TR_ASSERT(t, lineHasClass(line, kLanguageTokenClass::kRegular));

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    return kTR_Pass;
}

// Thematic break '---' -> kRule (and not a list marker)
DLL_EXPORT int test_markdown_rule(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    auto node = workspace->NewDocument("test.md");
    auto buffer = node->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("---");
    buffer->Reparse();

    auto line = buffer->LineAt(1);
    TR_ASSERT(t, lineHasClass(line, kLanguageTokenClass::kRule));
    TR_ASSERT(t, !lineHasClass(line, kLanguageTokenClass::kListMarker));

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    return kTR_Pass;
}

// A '# heading-looking' line INSIDE a fence must stay code, not become a heading (the fence guard).
DLL_EXPORT int test_markdown_heading_in_fence_is_code(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    auto node = workspace->NewDocument("test.md");
    auto buffer = node->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("```");
    buffer->AddLineUTF8("# not a heading");
    buffer->AddLineUTF8("```");
    buffer->Reparse();

    auto inside = buffer->LineAt(2);
    TR_ASSERT(t, !lineHasClass(inside, kLanguageTokenClass::kHeading));
    TR_ASSERT(t, lineHasClass(inside, kLanguageTokenClass::kCode));

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    return kTR_Pass;
}

// The fence must CLOSE, not just persist: prose after the closing ``` returns to normal interpretation
// (a '# Heading' below the fence is a heading again, not code). Locks the pop side of the cross-line state.
DLL_EXPORT int test_markdown_fence_closes(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    auto node = workspace->NewDocument("test.md");
    auto buffer = node->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("```");
    buffer->AddLineUTF8("code here");
    buffer->AddLineUTF8("```");
    buffer->AddLineUTF8("# After fence");
    buffer->Reparse();

    auto after = buffer->LineAt(4);
    TR_ASSERT(t, lineHasClass(after, kLanguageTokenClass::kHeading));
    TR_ASSERT(t, !lineHasClass(after, kLanguageTokenClass::kCode));

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    return kTR_Pass;
}

// Distinction property: '**bold**' is kStrong and NOT also kEmphasis (longest-match-first makes the
// 2-char '**' delimiter win over the 1-char '*').
DLL_EXPORT int test_markdown_strong_not_emphasis(ITesting *t) {
    auto workspace = Editor::Instance().GetWorkspace();
    auto node = workspace->NewDocument("test.md");
    auto buffer = node->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);

    buffer->AddLineUTF8("a **bold** b");
    buffer->Reparse();

    auto line = buffer->LineAt(1);
    TR_ASSERT(t, lineHasClass(line, kLanguageTokenClass::kStrong));
    TR_ASSERT(t, !lineHasClass(line, kLanguageTokenClass::kEmphasis));

    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    return kTR_Pass;
}

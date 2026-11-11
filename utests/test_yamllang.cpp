//
// Created by gnilk on 11.11.26.
//
// YAML language tests - Phase 1 of docs/support-yaml.md (comments + quoted strings). Asserts the class at
// a character position (the property), not attribute counts.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/Document.h"
#include "Core/Language/LanguageBase.h"
#include "Core/TextBuffer.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_yamllang(ITesting *t);
DLL_EXPORT int test_yamllang_detect(ITesting *t);
DLL_EXPORT int test_yamllang_comment(ITesting *t);
DLL_EXPORT int test_yamllang_comment_after_value(ITesting *t);
DLL_EXPORT int test_yamllang_hash_inside_word(ITesting *t);
DLL_EXPORT int test_yamllang_dqstring(ITesting *t);
DLL_EXPORT int test_yamllang_dqstring_escape(ITesting *t);
DLL_EXPORT int test_yamllang_sqstring_doubled(ITesting *t);
DLL_EXPORT int test_yamllang_apostrophe_in_word(ITesting *t);
DLL_EXPORT int test_yamllang_hash_in_string(ITesting *t);
DLL_EXPORT int test_yamllang_unclosed_quote_no_leak(ITesting *t);
}

// The class effective at char index 'pos' (attributes are ascending spans).
static kLanguageTokenClass classAt(const Line::Ref &line, int pos) {
    kLanguageTokenClass cls = kLanguageTokenClass::kUnknown;
    for (auto &a : line->Attributes()) {
        if (a.idxOrigString > pos) {
            break;
        }
        cls = a.tokenClass;
    }
    return cls;
}

static bool isComment(kLanguageTokenClass cls) {
    return (cls == kLanguageTokenClass::kLineComment) || (cls == kLanguageTokenClass::kCommentedText);
}

// Parse 'lines' as a .yml document and hand the buffer to 'check' (returns a kTR_* result).
template<typename F>
static int withYaml(ITesting *t, std::initializer_list<const char *> lines, F check) {
    auto workspace = Editor::Instance().GetWorkspace();
    TR_ASSERT(t, workspace != nullptr);
    auto node = workspace->NewDocument("test.yml");
    TR_ASSERT(t, node != nullptr);
    auto buffer = node->GetTextBuffer();
    TR_ASSERT(t, buffer != nullptr);
    for (auto l : lines) {
        buffer->AddLineUTF8(l);
    }
    buffer->Reparse();
    int result = check(buffer);
    TR_ASSERT(t, workspace->RemoveDocument(node->GetDocument()));
    return result;
}

DLL_EXPORT int test_yamllang(ITesting *t) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);
    return kTR_Pass;
}

// .yml and .yaml route to the YAML language, with '#' as line comment
DLL_EXPORT int test_yamllang_detect(ITesting *t) {
    auto yml = Editor::Instance().GetLanguageForExtension(".yml");
    auto yaml = Editor::Instance().GetLanguageForExtension(".yaml");
    TR_ASSERT(t, yml != nullptr);
    TR_ASSERT(t, yml == yaml);
    TR_ASSERT(t, yml->Identifier() == U"yaml");
    TR_ASSERT(t, yml->GetLineComment() == U"#");
    return kTR_Pass;
}

DLL_EXPORT int test_yamllang_comment(ITesting *t) {
    return withYaml(t, {"# a comment: with colon"}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, isComment(classAt(line, 0)));
        TR_ASSERT(t, isComment(classAt(line, 12)));   // the ':' is commented text, not an operator
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_comment_after_value(ITesting *t) {
    return withYaml(t, {"key: value # trailing"}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, !isComment(classAt(line, 5)));   // 'value'
        TR_ASSERT(t, isComment(classAt(line, 11)));    // '#'
        TR_ASSERT(t, isComment(classAt(line, 13)));    // 'trailing'
        return kTR_Pass;
    });
}

// '#' only starts a comment at the start of a token - 'url#frag' is a plain scalar
DLL_EXPORT int test_yamllang_hash_inside_word(ITesting *t) {
    return withYaml(t, {"link: url#frag"}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, !isComment(classAt(line, 9)));
        TR_ASSERT(t, !isComment(classAt(line, 10)));
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_dqstring(ITesting *t) {
    return withYaml(t, {"name: \"hello world\" tail"}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, classAt(line, 7) == kLanguageTokenClass::kString);    // 'hello'
        TR_ASSERT(t, classAt(line, 13) == kLanguageTokenClass::kString);   // 'world'
        TR_ASSERT(t, classAt(line, 20) != kLanguageTokenClass::kString);   // 'tail' - the string closed
        return kTR_Pass;
    });
}

// An escaped quote does not close a double-quoted string
DLL_EXPORT int test_yamllang_dqstring_escape(ITesting *t) {
    //                  0123456 7 8 9 0 1
    return withYaml(t, {"a: \"x\\\"y\" tail"}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        // a: "x\"y" tail  ->  'y' (idx 7) is still inside, 'tail' (idx 10) is outside
        TR_ASSERT(t, classAt(line, 7) == kLanguageTokenClass::kString);
        TR_ASSERT(t, classAt(line, 10) != kLanguageTokenClass::kString);
        return kTR_Pass;
    });
}

// 'it''s' is ONE single-quoted string (YAML doubles the quote to escape it)
DLL_EXPORT int test_yamllang_sqstring_doubled(ITesting *t) {
    return withYaml(t, {"a: 'it''s' tail"}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, classAt(line, 8) == kLanguageTokenClass::kString);    // 's'
        TR_ASSERT(t, classAt(line, 11) != kLanguageTokenClass::kString);   // 'tail'
        return kTR_Pass;
    });
}

// An apostrophe inside a plain scalar does not open a string (and so can't swallow the comment)
DLL_EXPORT int test_yamllang_apostrophe_in_word(ITesting *t) {
    return withYaml(t, {"msg: don't stop # c"}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, classAt(line, 11) != kLanguageTokenClass::kString);   // 'stop'
        TR_ASSERT(t, isComment(classAt(line, 16)));
        return kTR_Pass;
    });
}

// A '#' inside a quoted string is string, not a comment
DLL_EXPORT int test_yamllang_hash_in_string(ITesting *t) {
    return withYaml(t, {"color: \"#ff0000\""}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, classAt(line, 8) == kLanguageTokenClass::kString);
        TR_ASSERT(t, classAt(line, 9) == kLanguageTokenClass::kString);
        return kTR_Pass;
    });
}

// An unclosed quote ends at end of line - the next line is plain again (D3)
DLL_EXPORT int test_yamllang_unclosed_quote_no_leak(ITesting *t) {
    return withYaml(t, {"a: 'oops", "b: plain"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(1), 5) == kLanguageTokenClass::kString);
        auto next = buffer->LineAt(2);
        TR_ASSERT(t, classAt(next, 0) != kLanguageTokenClass::kString);
        TR_ASSERT(t, classAt(next, 3) != kLanguageTokenClass::kString);
        TR_ASSERT(t, next->GetStateStackDepth() == LangLineTokenizer::RootStateDepth);
        return kTR_Pass;
    });
}

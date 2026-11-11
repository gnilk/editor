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
DLL_EXPORT int test_yamllang_key(ITesting *t);
DLL_EXPORT int test_yamllang_key_nested(ITesting *t);
DLL_EXPORT int test_yamllang_key_quoted(ITesting *t);
DLL_EXPORT int test_yamllang_key_not_url(ITesting *t);
DLL_EXPORT int test_yamllang_key_not_in_string(ITesting *t);
DLL_EXPORT int test_yamllang_key_not_in_comment(ITesting *t);
DLL_EXPORT int test_yamllang_listmarker(ITesting *t);
DLL_EXPORT int test_yamllang_listmarker_not_negative(ITesting *t);
DLL_EXPORT int test_yamllang_keyword_whole_value(ITesting *t);
DLL_EXPORT int test_yamllang_number_whole_value(ITesting *t);
DLL_EXPORT int test_yamllang_typed_list_item(ITesting *t);
DLL_EXPORT int test_yamllang_document_marker(ITesting *t);
DLL_EXPORT int test_yamllang_anchor_alias_tag(ITesting *t);
DLL_EXPORT int test_yamllang_flow_mapping(ITesting *t);
DLL_EXPORT int test_yamllang_flow_sequence(ITesting *t);
DLL_EXPORT int test_yamllang_comment_survives_post_pass(ITesting *t);
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

//
// Phase 2 - the post-pass (keys, list markers, typed scalars, ...)
//
static const auto kKey = kLanguageTokenClass::kKey;

DLL_EXPORT int test_yamllang_key(ITesting *t) {
    return withYaml(t, {"name: value"}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, classAt(line, 0) == kKey);
        TR_ASSERT(t, classAt(line, 3) == kKey);
        TR_ASSERT(t, classAt(line, 4) != kKey);     // ':'
        TR_ASSERT(t, classAt(line, 6) != kKey);     // 'value'
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_key_nested(ITesting *t) {
    return withYaml(t, {"outer:", "  inner: 1", "  with space: x"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(1), 0) == kKey);
        TR_ASSERT(t, classAt(buffer->LineAt(2), 2) == kKey);
        TR_ASSERT(t, classAt(buffer->LineAt(3), 2) == kKey);
        TR_ASSERT(t, classAt(buffer->LineAt(3), 7) == kKey);    // 'space' - a key may contain blanks
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_key_quoted(ITesting *t) {
    return withYaml(t, {"\"a b\": 1"}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, classAt(line, 0) == kKey);
        TR_ASSERT(t, classAt(line, 2) == kKey);
        return kTR_Pass;
    });
}

// A ':' not followed by a blank is not a key separator - 'url: http://x' has ONE key
DLL_EXPORT int test_yamllang_key_not_url(ITesting *t) {
    return withYaml(t, {"url: http://x", "time: 12:30"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(1), 0) == kKey);
        TR_ASSERT(t, classAt(buffer->LineAt(1), 5) != kKey);     // 'http'
        TR_ASSERT(t, classAt(buffer->LineAt(2), 6) != kKey);     // '12'
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_key_not_in_string(ITesting *t) {
    return withYaml(t, {"- \"a: b\""}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, classAt(line, 3) == kLanguageTokenClass::kString);
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_key_not_in_comment(ITesting *t) {
    return withYaml(t, {"# key: value", "a: b # c: d"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, isComment(classAt(buffer->LineAt(1), 2)));
        TR_ASSERT(t, classAt(buffer->LineAt(2), 0) == kKey);
        TR_ASSERT(t, isComment(classAt(buffer->LineAt(2), 7)));
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_listmarker(ITesting *t) {
    return withYaml(t, {"- item", "  - key: v", "- - nested"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(1), 0) == kLanguageTokenClass::kListMarker);
        TR_ASSERT(t, classAt(buffer->LineAt(1), 2) != kLanguageTokenClass::kListMarker);
        TR_ASSERT(t, classAt(buffer->LineAt(2), 2) == kLanguageTokenClass::kListMarker);
        TR_ASSERT(t, classAt(buffer->LineAt(2), 4) == kKey);
        TR_ASSERT(t, classAt(buffer->LineAt(3), 0) == kLanguageTokenClass::kListMarker);
        TR_ASSERT(t, classAt(buffer->LineAt(3), 2) == kLanguageTokenClass::kListMarker);
        return kTR_Pass;
    });
}

// '-5' is a number, '--flag' is text; neither is a list marker
DLL_EXPORT int test_yamllang_listmarker_not_negative(ITesting *t) {
    return withYaml(t, {"-5", "--flag"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(1), 0) == kLanguageTokenClass::kNumber);
        TR_ASSERT(t, classAt(buffer->LineAt(2), 0) != kLanguageTokenClass::kListMarker);
        return kTR_Pass;
    });
}

// true/null/yes only when they are the WHOLE value
DLL_EXPORT int test_yamllang_keyword_whole_value(ITesting *t) {
    return withYaml(t, {"a: true", "b: true story", "c: Yes", "d: ~", "e: null # c"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(1), 3) == kLanguageTokenClass::kKeyword);
        TR_ASSERT(t, classAt(buffer->LineAt(2), 3) != kLanguageTokenClass::kKeyword);
        TR_ASSERT(t, classAt(buffer->LineAt(3), 3) == kLanguageTokenClass::kKeyword);
        TR_ASSERT(t, classAt(buffer->LineAt(4), 3) == kLanguageTokenClass::kKeyword);
        TR_ASSERT(t, classAt(buffer->LineAt(5), 3) == kLanguageTokenClass::kKeyword);
        TR_ASSERT(t, isComment(classAt(buffer->LineAt(5), 8)));
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_number_whole_value(ITesting *t) {
    return withYaml(t, {"a: 8080", "b: -1.5e3", "c: 0x1F", "d: .inf", "e: 1.2.3", "f: 3dfx", "g: \"42\""},
                    [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(1), 3) == kLanguageTokenClass::kNumber);
        TR_ASSERT(t, classAt(buffer->LineAt(2), 3) == kLanguageTokenClass::kNumber);
        TR_ASSERT(t, classAt(buffer->LineAt(3), 3) == kLanguageTokenClass::kNumber);
        TR_ASSERT(t, classAt(buffer->LineAt(4), 3) == kLanguageTokenClass::kNumber);
        TR_ASSERT(t, classAt(buffer->LineAt(5), 3) != kLanguageTokenClass::kNumber);    // a version
        TR_ASSERT(t, classAt(buffer->LineAt(6), 3) != kLanguageTokenClass::kNumber);
        TR_ASSERT(t, classAt(buffer->LineAt(7), 4) == kLanguageTokenClass::kString);    // quoted stays string
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_typed_list_item(ITesting *t) {
    return withYaml(t, {"- 42", "- false"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(1), 2) == kLanguageTokenClass::kNumber);
        TR_ASSERT(t, classAt(buffer->LineAt(2), 2) == kLanguageTokenClass::kKeyword);
        return kTR_Pass;
    });
}

// '---' / '...' at column 0 only; '%YAML' is a directive
DLL_EXPORT int test_yamllang_document_marker(ITesting *t) {
    return withYaml(t, {"---", "...", "  ---x", "%YAML 1.2"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(1), 0) == kLanguageTokenClass::kRule);
        TR_ASSERT(t, classAt(buffer->LineAt(2), 0) == kLanguageTokenClass::kRule);
        TR_ASSERT(t, classAt(buffer->LineAt(3), 2) != kLanguageTokenClass::kRule);
        TR_ASSERT(t, classAt(buffer->LineAt(4), 0) == kLanguageTokenClass::kPreProcessor);
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_anchor_alias_tag(ITesting *t) {
    //                   0         1
    //                   0123456789012345
    return withYaml(t, {"base: &def 5", "copy: *def", "n: !!str 5"}, [t](TextBuffer::Ref buffer) -> int {
        auto anchor = buffer->LineAt(1);
        TR_ASSERT(t, classAt(anchor, 6) == kLanguageTokenClass::kMacroIdentifier);
        TR_ASSERT(t, classAt(anchor, 11) == kLanguageTokenClass::kNumber);        // the value after the anchor
        TR_ASSERT(t, classAt(buffer->LineAt(2), 6) == kLanguageTokenClass::kMacroIdentifier);
        auto tag = buffer->LineAt(3);
        TR_ASSERT(t, classAt(tag, 3) == kLanguageTokenClass::kMacroIdentifier);
        TR_ASSERT(t, classAt(tag, 9) == kLanguageTokenClass::kNumber);
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_flow_mapping(ITesting *t) {
    //                   0         1         2
    //                   0123456789012345678901234567
    return withYaml(t, {"with: { node: 20, ok: true }"}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, classAt(line, 0) == kKey);
        TR_ASSERT(t, classAt(line, 8) == kKey);                                  // 'node'
        TR_ASSERT(t, classAt(line, 14) == kLanguageTokenClass::kNumber);         // '20'
        TR_ASSERT(t, classAt(line, 18) == kKey);                                 // 'ok'
        TR_ASSERT(t, classAt(line, 22) == kLanguageTokenClass::kKeyword);        // 'true'
        TR_ASSERT(t, classAt(line, 27) == kLanguageTokenClass::kOperator);       // '}'
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_flow_sequence(ITesting *t) {
    //                   0         1
    //                   01234567890123456789
    return withYaml(t, {"os: [linux, 3, \"a,b\"]"}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, classAt(line, 5) != kKey);                                  // 'linux' is a value
        TR_ASSERT(t, classAt(line, 12) == kLanguageTokenClass::kNumber);
        TR_ASSERT(t, classAt(line, 17) == kLanguageTokenClass::kString);         // the ',' inside the string
        return kTR_Pass;
    });
}

// Reclassifying the key must not swallow the trailing comment's spans
DLL_EXPORT int test_yamllang_comment_survives_post_pass(ITesting *t) {
    return withYaml(t, {"- key: 1 # note"}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, classAt(line, 7) == kLanguageTokenClass::kNumber);
        TR_ASSERT(t, isComment(classAt(line, 9)));
        TR_ASSERT(t, isComment(classAt(line, 12)));
        return kTR_Pass;
    });
}

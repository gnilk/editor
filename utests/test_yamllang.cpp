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
#include "Core/Language/LangLineTokenizer.h"

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
DLL_EXPORT int test_yamllang_block_body_is_string(ITesting *t);
DLL_EXPORT int test_yamllang_block_blank_line_inside(ITesting *t);
DLL_EXPORT int test_yamllang_block_ends_at_sibling_key(ITesting *t);
DLL_EXPORT int test_yamllang_block_list_item_key(ITesting *t);
DLL_EXPORT int test_yamllang_block_list_item_bare(ITesting *t);
DLL_EXPORT int test_yamllang_block_header_variants(ITesting *t);
DLL_EXPORT int test_yamllang_block_not_mid_value(ITesting *t);
DLL_EXPORT int test_yamllang_block_header_with_comment(ITesting *t);
DLL_EXPORT int test_yamllang_block_region_edit_inside(ITesting *t);
DLL_EXPORT int test_yamllang_block_region_remove_header(ITesting *t);
DLL_EXPORT int test_yamllang_block_tokenizetext_agrees(ITesting *t);
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
static const auto kConfigKey = kLanguageTokenClass::kConfigKey;

DLL_EXPORT int test_yamllang_key(ITesting *t) {
    return withYaml(t, {"name: value"}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, classAt(line, 0) == kConfigKey);
        TR_ASSERT(t, classAt(line, 3) == kConfigKey);
        TR_ASSERT(t, classAt(line, 4) != kConfigKey);     // ':'
        TR_ASSERT(t, classAt(line, 6) != kConfigKey);     // 'value'
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_key_nested(ITesting *t) {
    return withYaml(t, {"outer:", "  inner: 1", "  with space: x"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(1), 0) == kConfigKey);
        TR_ASSERT(t, classAt(buffer->LineAt(2), 2) == kConfigKey);
        TR_ASSERT(t, classAt(buffer->LineAt(3), 2) == kConfigKey);
        TR_ASSERT(t, classAt(buffer->LineAt(3), 7) == kConfigKey);    // 'space' - a key may contain blanks
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_key_quoted(ITesting *t) {
    return withYaml(t, {"\"a b\": 1"}, [t](TextBuffer::Ref buffer) -> int {
        auto line = buffer->LineAt(1);
        TR_ASSERT(t, classAt(line, 0) == kConfigKey);
        TR_ASSERT(t, classAt(line, 2) == kConfigKey);
        return kTR_Pass;
    });
}

// A ':' not followed by a blank is not a key separator - 'url: http://x' has ONE key
DLL_EXPORT int test_yamllang_key_not_url(ITesting *t) {
    return withYaml(t, {"url: http://x", "time: 12:30"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(1), 0) == kConfigKey);
        TR_ASSERT(t, classAt(buffer->LineAt(1), 5) != kConfigKey);     // 'http'
        TR_ASSERT(t, classAt(buffer->LineAt(2), 6) != kConfigKey);     // '12'
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
        TR_ASSERT(t, classAt(buffer->LineAt(2), 0) == kConfigKey);
        TR_ASSERT(t, isComment(classAt(buffer->LineAt(2), 7)));
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_listmarker(ITesting *t) {
    return withYaml(t, {"- item", "  - key: v", "- - nested"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(1), 0) == kLanguageTokenClass::kListMarker);
        TR_ASSERT(t, classAt(buffer->LineAt(1), 2) != kLanguageTokenClass::kListMarker);
        TR_ASSERT(t, classAt(buffer->LineAt(2), 2) == kLanguageTokenClass::kListMarker);
        TR_ASSERT(t, classAt(buffer->LineAt(2), 4) == kConfigKey);
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
        TR_ASSERT(t, classAt(line, 0) == kConfigKey);
        TR_ASSERT(t, classAt(line, 8) == kConfigKey);                                  // 'node'
        TR_ASSERT(t, classAt(line, 14) == kLanguageTokenClass::kNumber);         // '20'
        TR_ASSERT(t, classAt(line, 18) == kConfigKey);                                 // 'ok'
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
        TR_ASSERT(t, classAt(line, 5) != kConfigKey);                                  // 'linux' is a value
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

//
// Phase 3 - block scalars: an indent-scoped tokenizer state, pushed at end of a 'key: |' line
//
static const auto kString = kLanguageTokenClass::kString;

DLL_EXPORT int test_yamllang_block_body_is_string(ITesting *t) {
    return withYaml(t, {"run: |", "  foo: bar", "  # not a comment", "next: 1"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(1), 0) == kConfigKey);
        TR_ASSERT(t, classAt(buffer->LineAt(2), 2) == kString);          // 'foo:' is text, not a key
        TR_ASSERT(t, classAt(buffer->LineAt(3), 2) == kString);          // '#' inside the body is text
        TR_ASSERT(t, classAt(buffer->LineAt(4), 0) == kConfigKey);             // the body ended
        TR_ASSERT(t, buffer->LineAt(4)->GetStateStackDepth() == LangLineTokenizer::RootStateDepth);
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_block_blank_line_inside(ITesting *t) {
    return withYaml(t, {"text: >", "  one", "", "   ", "  two: x", "next: 1"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(5), 2) == kString);
        TR_ASSERT(t, classAt(buffer->LineAt(6), 0) == kConfigKey);
        return kTR_Pass;
    });
}

// The GitHub Actions shape: a sibling key at the key's column ends the body, even though it is indented
// deeper than the list dash
DLL_EXPORT int test_yamllang_block_ends_at_sibling_key(ITesting *t) {
    return withYaml(t, {"steps:", "  - run: |", "      make", "    name: build", "  - run: x"},
                    [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(3), 6) == kString);
        TR_ASSERT(t, classAt(buffer->LineAt(4), 4) == kConfigKey);
        TR_ASSERT(t, classAt(buffer->LineAt(5), 4) == kConfigKey);
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_block_list_item_key(ITesting *t) {
    return withYaml(t, {"- key: |", "    body: x", "  other: y"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(2), 4) == kString);
        TR_ASSERT(t, classAt(buffer->LineAt(3), 2) == kConfigKey);             // at the key's column: ended
        return kTR_Pass;
    });
}

// '- |' - the body only has to be deeper than the dash
DLL_EXPORT int test_yamllang_block_list_item_bare(ITesting *t) {
    return withYaml(t, {"- |", "  body: x", "- next: y"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(2), 2) == kString);
        TR_ASSERT(t, classAt(buffer->LineAt(3), 2) == kConfigKey);
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_block_header_variants(ITesting *t) {
    return withYaml(t, {"a: |-", "  x: 1", "b: >+", "  y: 2", "c: 1"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(2), 2) == kString);
        TR_ASSERT(t, classAt(buffer->LineAt(3), 0) == kConfigKey);
        TR_ASSERT(t, classAt(buffer->LineAt(4), 2) == kString);
        TR_ASSERT(t, classAt(buffer->LineAt(5), 0) == kConfigKey);
        return kTR_Pass;
    });
}

// '|' / '>' followed by anything but a comment is not a header: 'a > b', '| "x"'
DLL_EXPORT int test_yamllang_block_not_mid_value(ITesting *t) {
    return withYaml(t, {"cond: a > b", "  key: 1", "pipe: | \"x\"", "  key: 2"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, classAt(buffer->LineAt(2), 2) == kConfigKey);
        TR_ASSERT(t, classAt(buffer->LineAt(4), 2) == kConfigKey);
        return kTR_Pass;
    });
}

DLL_EXPORT int test_yamllang_block_header_with_comment(ITesting *t) {
    return withYaml(t, {"run: | # the script", "  key: 1", "next: 1"}, [t](TextBuffer::Ref buffer) -> int {
        TR_ASSERT(t, isComment(classAt(buffer->LineAt(1), 7)));
        TR_ASSERT(t, classAt(buffer->LineAt(2), 2) == kString);
        TR_ASSERT(t, classAt(buffer->LineAt(3), 0) == kConfigKey);
        return kTR_Pass;
    });
}

// Enough lines around the block that the region parse really seeks (it covers everything near the edges)
static std::vector<const char *> regionDocument = {
    "a1: 1", "a2: 1", "a3: 1", "a4: 1", "a5: 1", "a6: 1", "a7: 1",
    "run: |",                       // line 8
    "  one: x", "  two: x", "  three: x", "  four: x",
    "after: 1",                     // line 13
    "b1: 1", "b2: 1", "b3: 1", "b4: 1", "b5: 1", "b6: 1", "b7: 1", "b8: 1",
};

static TextBuffer::Ref loadRegionDocument(Document::Ref &outDocument) {
    auto node = Editor::Instance().GetWorkspace()->NewDocument("region.yml");
    outDocument = node->GetDocument();
    auto buffer = node->GetTextBuffer();
    for (auto l : regionDocument) {
        buffer->AddLineUTF8(l);
    }
    buffer->Reparse();
    return buffer;
}

// An edit inside the body reparses from before the header (the body lines are not clean) - still text
DLL_EXPORT int test_yamllang_block_region_edit_inside(ITesting *t) {
    Document::Ref document;
    auto buffer = loadRegionDocument(document);
    TR_ASSERT(t, classAt(buffer->LineAt(10), 2) == kString);

    buffer->LineAt(10)->Insert(2, U"new: ");
    buffer->ReparseRegion(10, 11);
    TR_ASSERT(t, classAt(buffer->LineAt(10), 2) == kString);           // 'new:' is body text, not a key
    TR_ASSERT(t, classAt(buffer->LineAt(13), 0) == kConfigKey);

    TR_ASSERT(t, Editor::Instance().GetWorkspace()->RemoveDocument(document));
    return kTR_Pass;
}

// Removing the header turns the old body back into YAML - the region must run past the old body
DLL_EXPORT int test_yamllang_block_region_remove_header(ITesting *t) {
    Document::Ref document;
    auto buffer = loadRegionDocument(document);

    buffer->LineAt(8)->Delete(5, 1);                // 'run: |' -> 'run: '
    buffer->ReparseRegion(8, 9);
    for (size_t idxLine = 9; idxLine <= 12; idxLine++) {
        TR_ASSERT(t, classAt(buffer->LineAt(idxLine), 2) == kConfigKey);
        TR_ASSERT(t, buffer->LineAt(idxLine)->GetStateStackDepth() == LangLineTokenizer::RootStateDepth);
    }

    // ...and putting it back makes it a body again
    buffer->LineAt(8)->Insert(5, U"|");
    buffer->ReparseRegion(8, 9);
    for (size_t idxLine = 9; idxLine <= 12; idxLine++) {
        TR_ASSERT(t, classAt(buffer->LineAt(idxLine), 2) == kString);
    }
    TR_ASSERT(t, classAt(buffer->LineAt(13), 0) == kConfigKey);

    TR_ASSERT(t, Editor::Instance().GetWorkspace()->RemoveDocument(document));
    return kTR_Pass;
}

// The index fast path pops the scope at the same line as the highlighter
DLL_EXPORT int test_yamllang_block_tokenizetext_agrees(ITesting *t) {
    auto tokenizer = Editor::Instance().GetLanguageForExtension(".yml")->Tokenizer();
    std::vector<std::pair<size_t, LangToken>> tokens;
    tokenizer.TokenizeText(U"run: |\n  foo: bar\n\n  baz\nnext: 1\n", [&tokens](size_t idxLine, const LangToken &token) {
        tokens.push_back({idxLine, token});
    });
    bool sawNext = false;
    for (auto &[idxLine, token] : tokens) {
        if ((idxLine == 1) || (idxLine == 3)) {
            TR_ASSERT(t, token.classification == kString);
        }
        if (idxLine == 4) {
            TR_ASSERT(t, token.classification != kString);
            sawNext = true;
        }
    }
    TR_ASSERT(t, sawNext);
    return kTR_Pass;
}

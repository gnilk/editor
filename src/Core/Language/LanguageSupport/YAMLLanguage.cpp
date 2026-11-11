//
// Created by gnilk on 11.11.26.
//
// YAML (.yml / .yaml) syntax support - Phase 1 of docs/support-yaml.md: comments and quoted strings.
//
// Identifiers only match at the start of a token (after whitespace or a postfix char), which is exactly
// where YAML lets a comment or a quoted scalar begin. That is why '#' and the quotes are deliberately NOT
// postfix chars in 'main': 'url#frag' is not a comment and "don't" is not a string.
//

#include "YAMLLanguage.h"

using namespace gedit;

// state: main
static const std::u32string yamlOperators = U": , { } [ ]";
// What ends a plain word: 'key:' is 'key' + ':', '[a,b]' splits at the commas. Never '#' or quotes (above).
static const std::u32string yamlMainPostfix = U": , { } [ ]";
static const std::u32string yamlComment = U"#";
// Both quotes open a string; longest-match is irrelevant here (one char each).
static const std::u32string yamlQuotes = U"\" '";

// state: in_dq_string - '\"' and '\\' are escapes that do NOT end the string. Length-descending so the
// two-char escapes win over the closing '"'.
static const std::u32string yamlDqTokens = U"\\\\ \\\" \"";
// state: in_sq_string - YAML escapes a single quote by doubling it ('it''s'). No escape token is needed:
// the first quote closes the string and the second, at token start, opens it again - same colours.
static const std::u32string yamlSqTokens = U"'";

bool YAMLLanguage::Initialize() {
    auto state = tokenizer.GetOrAddState("main");
    state->SetIdentifiers(kLanguageTokenClass::kOperator, yamlOperators);
    state->SetIdentifiers(kLanguageTokenClass::kLineComment, yamlComment);
    state->SetIdentifiers(kLanguageTokenClass::kString, yamlQuotes);
    state->SetPostFixIdentifiers(yamlMainPostfix);

    state->GetOrAddAction(U"#", LangLineTokenizer::kAction::kPushState, "in_comment");
    state->GetOrAddAction(U"\"", LangLineTokenizer::kAction::kPushState, "in_dq_string");
    state->GetOrAddAction(U"'", LangLineTokenizer::kAction::kPushState, "in_sq_string");

    // A comment runs to end of line.
    auto stateComment = tokenizer.GetOrAddState("in_comment");
    stateComment->SetRegularTokenClass(kLanguageTokenClass::kCommentedText);
    stateComment->SetEOLAction(LangLineTokenizer::kAction::kPopState);

    // Quoted strings end at end of line too (D3 in the plan): YAML allows multi-line quoted scalars, but an
    // unclosed quote must not colour the rest of the file.
    auto stateDq = tokenizer.GetOrAddState("in_dq_string");
    stateDq->SetRegularTokenClass(kLanguageTokenClass::kString);
    stateDq->SetIdentifiers(kLanguageTokenClass::kString, yamlDqTokens);
    stateDq->SetPostFixIdentifiers(yamlDqTokens);
    stateDq->GetOrAddAction(U"\"", LangLineTokenizer::kAction::kPopState);
    stateDq->SetEOLAction(LangLineTokenizer::kAction::kPopState);

    auto stateSq = tokenizer.GetOrAddState("in_sq_string");
    stateSq->SetRegularTokenClass(kLanguageTokenClass::kString);
    stateSq->SetIdentifiers(kLanguageTokenClass::kString, yamlSqTokens);
    stateSq->SetPostFixIdentifiers(yamlSqTokens);
    stateSq->GetOrAddAction(U"'", LangLineTokenizer::kAction::kPopState);
    stateSq->SetEOLAction(LangLineTokenizer::kAction::kPopState);

    tokenizer.SetStartState("main");

    // Grab configuration (if any) - also the key into autopairs.yml / indent.yml
    ConfigFromNodeName("yaml");

    return true;
}

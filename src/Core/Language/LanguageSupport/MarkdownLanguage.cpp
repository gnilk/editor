//
// Created by gnilk on 15.06.26.
//
// Markdown (.md / .markdown) syntax support.
//
// §2.A (inline + fenced spans) is implemented here: symmetric-delimiter constructs that map onto the
// same push/pop-state pattern used by CPP/JSON strings and block comments. The line-anchored block
// syntax (headings, lists, blockquotes) is NOT here - it needs beginning-of-line awareness the lexer
// doesn't have, and is handled in §2.B via OnPostProcessParsedLine. See docs/support-markdown.md.
//
// NOTE: emphasis/link handling is deliberately approximate (no CommonMark flanking rules) - see the
// non-goals in the plan. Intraword '_' emphasis is intentionally NOT enabled (snake_case would
// constantly false-trigger); only '*'/'**' drive emphasis for now.
//

#include "MarkdownLanguage.h"

using namespace gedit;

// state: main - the delimiters that open an inline/fenced span. Declared length-descending within each
// list so longest-match wins (``` before `, ** before *).
static const std::u32string mdCodeDelims = U"``` `";    // fenced (3) + inline (1) code
static const std::u32string mdStrongDelim = U"**";       // strong emphasis (bold)
static const std::u32string mdEmDelim = U"*";            // emphasis (italic)
static const std::u32string mdLinkDelims = U"[ ]";       // link text brackets
// postfix set: lets a regular word break directly before a delimiter (e.g. word**bold**).
static const std::u32string mdMainPostfix = U"``` ** * ` [ ]";

bool MarkdownLanguage::Initialize() {
    auto state = tokenizer.GetOrAddState("main");
    state->SetIdentifiers(kLanguageTokenClass::kCode, mdCodeDelims);
    state->SetIdentifiers(kLanguageTokenClass::kStrong, mdStrongDelim);
    state->SetIdentifiers(kLanguageTokenClass::kEmphasis, mdEmDelim);
    state->SetIdentifiers(kLanguageTokenClass::kLink, mdLinkDelims);
    state->SetPostFixIdentifiers(mdMainPostfix);

    state->GetOrAddAction(U"```", LangLineTokenizer::kAction::kPushState, "in_fence");
    state->GetOrAddAction(U"`", LangLineTokenizer::kAction::kPushState, "in_code_span");
    state->GetOrAddAction(U"**", LangLineTokenizer::kAction::kPushState, "in_strong");
    state->GetOrAddAction(U"*", LangLineTokenizer::kAction::kPushState, "in_em");
    state->GetOrAddAction(U"[", LangLineTokenizer::kAction::kPushState, "in_link");

    // in_fence: fenced code block. No EOL action - it persists across lines (like a CPP block comment)
    // until the closing fence. Everything inside is verbatim code.
    auto stateFence = tokenizer.GetOrAddState("in_fence");
    stateFence->SetRegularTokenClass(kLanguageTokenClass::kCode);
    stateFence->SetIdentifiers(kLanguageTokenClass::kCode, U"```");
    stateFence->SetPostFixIdentifiers(U"```");
    stateFence->GetOrAddAction(U"```", LangLineTokenizer::kAction::kPopState);

    // in_code_span: inline code `...`. Pops at EOL too - inline code does not span lines.
    auto stateCodeSpan = tokenizer.GetOrAddState("in_code_span");
    stateCodeSpan->SetRegularTokenClass(kLanguageTokenClass::kCode);
    stateCodeSpan->SetIdentifiers(kLanguageTokenClass::kCode, U"`");
    stateCodeSpan->SetPostFixIdentifiers(U"`");
    stateCodeSpan->GetOrAddAction(U"`", LangLineTokenizer::kAction::kPopState);
    stateCodeSpan->SetEOLAction(LangLineTokenizer::kAction::kPopState);

    // in_strong: **bold**. EOL-pop so an unterminated delimiter can't leak into the next line.
    auto stateStrong = tokenizer.GetOrAddState("in_strong");
    stateStrong->SetRegularTokenClass(kLanguageTokenClass::kStrong);
    stateStrong->SetIdentifiers(kLanguageTokenClass::kStrong, U"**");
    stateStrong->SetPostFixIdentifiers(U"**");
    stateStrong->GetOrAddAction(U"**", LangLineTokenizer::kAction::kPopState);
    stateStrong->SetEOLAction(LangLineTokenizer::kAction::kPopState);

    // in_em: *italic*. EOL-pop, same leak-guard as in_strong.
    auto stateEm = tokenizer.GetOrAddState("in_em");
    stateEm->SetRegularTokenClass(kLanguageTokenClass::kEmphasis);
    stateEm->SetIdentifiers(kLanguageTokenClass::kEmphasis, U"*");
    stateEm->SetPostFixIdentifiers(U"*");
    stateEm->GetOrAddAction(U"*", LangLineTokenizer::kAction::kPopState);
    stateEm->SetEOLAction(LangLineTokenizer::kAction::kPopState);

    // in_link: the [text] portion of a [text](url) link. The (url) trailer is left as regular text for
    // now. EOL-pop guards an unclosed bracket.
    auto stateLink = tokenizer.GetOrAddState("in_link");
    stateLink->SetRegularTokenClass(kLanguageTokenClass::kLink);
    stateLink->SetIdentifiers(kLanguageTokenClass::kLink, U"]");
    stateLink->SetPostFixIdentifiers(U"]");
    stateLink->GetOrAddAction(U"]", LangLineTokenizer::kAction::kPopState);
    stateLink->SetEOLAction(LangLineTokenizer::kAction::kPopState);

    tokenizer.SetStartState("main");

    // Grab configuration (if any) - also the key into autopairs.yml
    ConfigFromNodeName("markdown");

    return true;
}

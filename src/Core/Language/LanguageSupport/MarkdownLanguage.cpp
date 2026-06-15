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
#include "Core/Editor.h"

using namespace gedit;

// Forward declarations - the public method reads top-down, helpers follow.
static bool isThematicBreak(const std::u32string &buffer);
static Line::LineAttrib makeAttrib(int idx, kLanguageTokenClass cls);
static void reclassifyWholeLine(const Line::Ref &line, kLanguageTokenClass cls);
static void reclassifyRange(const Line::Ref &line, int start, int end, kLanguageTokenClass cls);

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

//
// §2.B - line-anchored block syntax. Runs after the generic per-line tokenization, inspecting the first
// non-whitespace token and rewriting attributes. The tokenizer has no beginning-of-line awareness, which
// is exactly why this lives here. See docs/support-markdown.md.
//
void MarkdownLanguage::OnPostProcessParsedLine(Line::Ref line) {
    // A line that STARTS inside a fenced code block is verbatim - never reinterpret it (a '#' or '-'
    // inside ``` is code, not a heading/bullet). Only in_fence persists across lines, so depth > root
    // means "inside a fence".
    if (line->GetStateStackDepth() > LangLineTokenizer::RootStateDepth) {
        return;
    }

    const auto &buffer = line->Buffer();
    int n = static_cast<int>(buffer.size());

    // First non-whitespace character.
    int i = 0;
    while ((i < n) && ((buffer[i] == U' ') || (buffer[i] == U'\t'))) {
        i++;
    }
    if (i >= n) {
        return;
    }
    char32_t c = buffer[i];

    // ATX heading: 1-6 '#' followed by a space or end-of-line. Whole line reads as a heading.
    if (c == U'#') {
        int h = 0;
        while ((i + h < n) && (buffer[i + h] == U'#')) {
            h++;
        }
        if ((h <= 6) && ((i + h >= n) || (buffer[i + h] == U' '))) {
            reclassifyWholeLine(line, kLanguageTokenClass::kHeading);
        }
        return;
    }

    // Thematic break (---, ***, ___) - takes priority over list/emphasis interpretation.
    if ((c == U'-') || (c == U'*') || (c == U'_')) {
        if (isThematicBreak(buffer)) {
            reclassifyWholeLine(line, kLanguageTokenClass::kRule);
            return;
        }
    }

    // Blockquote: '>' at line start - whole line muted.
    if (c == U'>') {
        reclassifyWholeLine(line, kLanguageTokenClass::kBlockQuote);
        return;
    }

    // Unordered list marker: '-', '*', '+' followed by a space.
    if ((c == U'-') || (c == U'*') || (c == U'+')) {
        if ((i + 1 < n) && (buffer[i + 1] == U' ')) {
            reclassifyRange(line, i, i + 1, kLanguageTokenClass::kListMarker);
            // A '*' bullet was mis-tokenized as emphasis (no line-start awareness in the lexer), so the
            // item text carries spurious kEmphasis spans - reset them to regular. '-'/'+' are not lexer
            // delimiters, so their item text is already correct and is left intact (inline code/emphasis
            // inside the item keeps its highlighting).
            if (c == U'*') {
                reclassifyRange(line, i + 1, n, kLanguageTokenClass::kRegular);
            }
        }
        return;
    }

    // Ordered list marker: digits then '.' or ')' then a space (or EOL).
    if ((c >= U'0') && (c <= U'9')) {
        int d = 0;
        while ((i + d < n) && (buffer[i + d] >= U'0') && (buffer[i + d] <= U'9')) {
            d++;
        }
        int marker = i + d;
        if ((marker < n) && ((buffer[marker] == U'.') || (buffer[marker] == U')')) &&
            ((marker + 1 >= n) || (buffer[marker + 1] == U' '))) {
            reclassifyRange(line, i, marker + 1, kLanguageTokenClass::kListMarker);
        }
        return;
    }
}

// A thematic break is >= 3 of the SAME marker char (-, *, _), spaces allowed, nothing else on the line.
static bool isThematicBreak(const std::u32string &buffer) {
    char32_t marker = 0;
    int count = 0;
    for (char32_t ch : buffer) {
        if ((ch == U' ') || (ch == U'\t')) {
            continue;
        }
        if ((ch == U'-') || (ch == U'*') || (ch == U'_')) {
            if (marker == 0) {
                marker = ch;
            } else if (ch != marker) {
                return false;
            }
            count++;
        } else {
            return false;
        }
    }
    return (count >= 3);
}

// Build a single attribute (with theme colors) for the given class at the given char index.
static Line::LineAttrib makeAttrib(int idx, kLanguageTokenClass cls) {
    Line::LineAttrib attrib;
    attrib.idxOrigString = idx;
    attrib.tokenClass = cls;
    auto [fg, bg] = Editor::Instance().ColorFromLanguageToken(cls);
    attrib.foregroundColor = fg;
    attrib.backgroundColor = bg;
    return attrib;
}

// The class effective at char position 'pos' in the existing (ascending) attribute list.
static kLanguageTokenClass classAt(const std::vector<Line::LineAttrib> &attribs, int pos) {
    kLanguageTokenClass cls = kLanguageTokenClass::kRegular;
    for (auto &a : attribs) {
        if (a.idxOrigString <= pos) {
            cls = a.tokenClass;
        } else {
            break;
        }
    }
    return cls;
}

// Replace the whole line's attributes with one span of 'cls'.
static void reclassifyWholeLine(const Line::Ref &line, kLanguageTokenClass cls) {
    auto &attribs = line->Attributes();
    attribs.clear();
    attribs.push_back(makeAttrib(0, cls));
}

// Override the half-open char range [start, end) with 'cls', preserving spans outside it. The tail span
// (at 'end') is restored to whatever class was effective there originally so later content keeps its
// highlighting.
static void reclassifyRange(const Line::Ref &line, int start, int end, kLanguageTokenClass cls) {
    auto &attribs = line->Attributes();
    int lineLen = static_cast<int>(line->Buffer().size());

    kLanguageTokenClass tailClass = classAt(attribs, end);

    std::vector<Line::LineAttrib> rebuilt;
    for (auto &a : attribs) {
        if (a.idxOrigString < start) {
            rebuilt.push_back(a);
        }
    }
    rebuilt.push_back(makeAttrib(start, cls));
    if (end < lineLen) {
        rebuilt.push_back(makeAttrib(end, tailClass));
        for (auto &a : attribs) {
            if (a.idxOrigString > end) {
                rebuilt.push_back(a);
            }
        }
    }
    attribs = rebuilt;
}

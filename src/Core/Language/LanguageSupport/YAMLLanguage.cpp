//
// Created by gnilk on 11.11.26.
//
// YAML (.yml / .yaml) syntax support (docs/support-yaml.md): comments and quoted strings in the tokenizer,
// the line-anchored structure in the post-pass at the end of this file.
//
// Identifiers only match at the start of a token (after whitespace or a postfix char), which is exactly
// where YAML lets a comment or a quoted scalar begin. That is why '#' and the quotes are deliberately NOT
// postfix chars in 'main': 'url#frag' is not a comment and "don't" is not a string.
//

#include "YAMLLanguage.h"
#include "Core/Language/LineAttribEdit.h"

using namespace gedit;

// Forward declarations - the methods read top-down, the helpers follow them.
static int blockScalarScopeIndent(const std::u32string &line);
static bool hasKeySeparator(std::u32string_view text);

// state: main. Block scalar headers first: the list matches in order, so '|-' must come before '|'.
static const std::u32string yamlBlockHeaders = U"|- |+ >- >+ | >";
static const std::u32string yamlOperators = U"|- |+ >- >+ | > : , { } [ ]";
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

    // Block scalars ('key: |' / 'key: >' and their chomping variants): only a header when it ends the line
    // (a comment may follow) - 'a > b' is plain text. The body is every following line indented deeper than
    // the key, blank lines included (§4 of the plan).
    std::vector<std::u32string> headers;
    strutil::splitToStringList(headers, yamlBlockHeaders);
    for (auto &header : headers) {
        state->GetOrAddAction(header, LangLineTokenizer::kAction::kPushStateAtEOL, "in_block_scalar");
    }
    auto stateBlock = tokenizer.GetOrAddState("in_block_scalar");
    stateBlock->SetRegularTokenClass(kLanguageTokenClass::kString);
    stateBlock->SetIndentScoped(true);
    tokenizer.SetScopeIndentFunction(blockScalarScopeIndent);

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

// On Enter after a list item: '- name: x' -> the next line lines up with 'name' (the item's next key);
// '- a' -> the default (back at the dash, ready for '- b'). Other lines keep the default too.
int YAMLLanguage::GetContinuationColumn(std::u32string_view lineBeforeCursor) {
    int n = static_cast<int>(lineBeforeCursor.size());
    auto isBlank = [](char32_t ch) { return (ch == U' ') || (ch == U'\t'); };
    int pos = 0;
    while ((pos < n) && isBlank(lineBeforeCursor[pos])) {
        pos++;
    }
    bool hasListMarker = false;
    while ((pos < n) && (lineBeforeCursor[pos] == U'-') && ((pos + 1 == n) || isBlank(lineBeforeCursor[pos + 1]))) {
        hasListMarker = true;
        pos++;
        while ((pos < n) && isBlank(lineBeforeCursor[pos])) {
            pos++;
        }
    }
    if (!hasListMarker || !hasKeySeparator(lineBeforeCursor.substr(pos))) {
        return -1;
    }
    return pos;
}

// A block key's ':' (followed by a blank or the end) outside quotes and before a comment.
static bool hasKeySeparator(std::u32string_view text) {
    char32_t quote = 0;
    for (size_t i = 0; i < text.size(); i++) {
        char32_t ch = text[i];
        if (quote != 0) {
            if (ch == quote) {
                quote = 0;
            }
            continue;
        }
        bool isTokenStart = (i == 0) || (text[i - 1] == U' ') || (text[i - 1] == U'\t');
        if (((ch == U'"') || (ch == U'\'')) && isTokenStart) {
            quote = ch;
        } else if ((ch == U'#') && isTokenStart) {
            return false;
        } else if ((ch == U':') && ((i + 1 == text.size()) || (text[i + 1] == U' ') || (text[i + 1] == U'\t'))) {
            return true;
        }
    }
    return false;
}

// The indent a block scalar's body must exceed: the column of the key that owns it. '- key: |' -> the key's
// column (past the list markers); '- |' -> the dash's column; 'key: |' -> the line's indent.
static int blockScalarScopeIndent(const std::u32string &line) {
    int n = static_cast<int>(line.size());
    auto isBlank = [](char32_t ch) { return (ch == U' ') || (ch == U'\t'); };
    int pos = 0;
    while ((pos < n) && isBlank(line[pos])) {
        pos++;
    }
    int lastDash = -1;
    while ((pos < n) && (line[pos] == U'-') && ((pos + 1 == n) || isBlank(line[pos + 1]))) {
        lastDash = pos;
        pos++;
        while ((pos < n) && isBlank(line[pos])) {
            pos++;
        }
    }
    bool isHeaderOnly = (pos < n) && ((line[pos] == U'|') || (line[pos] == U'>'));
    if (isHeaderOnly && (lastDash >= 0)) {
        return lastDash;
    }
    return pos;
}

//
// §2.B - the line-anchored part. YAML's line grammar is local to the line (block scalars aside, §4), so one
// pass over the line is enough: list markers, then 'key:', then the value. The tokenizer's spans tell which
// characters are string or comment; everything is rewritten through LineAttribEdit.
//
namespace {
    class YAMLLinePass {
    public:
        explicit YAMLLinePass(const Line::Ref &line);
        void Run();

    private:
        bool IsDocumentMarker() const;
        int SkipListMarkers(int pos);
        void ClassifyEntry(int pos);
        void ClassifyValue(int pos, int end);
        void ClassifyFlow(int start, int end);
        void ClassifyFlowPiece(int start, int end, bool isKey);
        void ClassifyNode(int pos, int end);
        int SkipProperties(int pos, int end);
        int FindKeySeparator(int start, int end) const;
        int SkipBlanks(int pos, int end) const;
        int TrimEnd(int start, int end) const;
        int TokenEnd(int pos, int end) const;
        bool IsOperatorAt(int pos) const;
        bool IsQuotedAt(int pos) const;
        void Reclassify(int start, int end, kLanguageTokenClass cls);

        static bool IsBlank(char32_t ch);
        static kLanguageTokenClass TypeOfScalar(const std::u32string &text);
        static bool IsNumber(const std::u32string &text);
        static bool IsDigitInBase(char32_t ch, int base);

    private:
        Line::Ref line;
        const std::u32string &buffer;
        std::vector<Line::LineAttrib> tokenSpans;    // the tokenizer's spans, before this pass rewrites any
        int commentStart = 0;                        // first char of a trailing comment, or the line length
    };
}

void YAMLLanguage::OnPostProcessParsedLine(Line::Ref line) {
    // A line that starts inside a multi-line construct (a block scalar, §4) is not YAML structure.
    if (line->GetStateStackDepth() > LangLineTokenizer::RootStateDepth) {
        return;
    }
    YAMLLinePass pass(line);
    pass.Run();
}

YAMLLinePass::YAMLLinePass(const Line::Ref &line) : line(line), buffer(line->Buffer()), tokenSpans(line->Attributes()) {
    commentStart = static_cast<int>(buffer.size());
    for (auto &span : tokenSpans) {
        if ((span.tokenClass == kLanguageTokenClass::kLineComment) || (span.tokenClass == kLanguageTokenClass::kCommentedText)) {
            commentStart = span.idxOrigString;
            break;
        }
    }
}

void YAMLLinePass::Run() {
    int pos = SkipBlanks(0, commentStart);
    if (pos >= commentStart) {
        return;
    }
    // Document markers and directives only exist at column 0.
    if ((pos == 0) && IsDocumentMarker()) {
        Reclassify(0, 3, kLanguageTokenClass::kRule);
        return;
    }
    if ((pos == 0) && (buffer[0] == U'%')) {
        Reclassify(0, TrimEnd(0, commentStart), kLanguageTokenClass::kPreProcessor);
        return;
    }
    pos = SkipListMarkers(pos);
    if (pos < commentStart) {
        ClassifyEntry(pos);
    }
}

// '---' (document start) or '...' (document end), alone or followed by a blank.
bool YAMLLinePass::IsDocumentMarker() const {
    auto n = buffer.size();
    if (n < 3) {
        return false;
    }
    bool isDashes = (buffer.compare(0, 3, U"---") == 0);
    bool isDots = (buffer.compare(0, 3, U"...") == 0);
    return (isDashes || isDots) && ((n == 3) || IsBlank(buffer[3]));
}

// '- ' prefixes, possibly several ('- - a' is a list in a list). Returns the first position after them.
int YAMLLinePass::SkipListMarkers(int pos) {
    int n = static_cast<int>(buffer.size());
    while ((pos < commentStart) && (buffer[pos] == U'-') && !IsQuotedAt(pos)) {
        if ((pos + 1 < n) && !IsBlank(buffer[pos + 1])) {
            break;          // '-5', '--flag': a scalar, not a marker
        }
        Reclassify(pos, pos + 1, kLanguageTokenClass::kListMarker);
        pos = SkipBlanks(pos + 1, commentStart);
    }
    return pos;
}

// 'key: value', or just a value (a list item, a continuation line).
void YAMLLinePass::ClassifyEntry(int pos) {
    int sep = FindKeySeparator(pos, commentStart);
    if (sep >= 0) {
        int keyEnd = TrimEnd(pos, sep);
        if (keyEnd > pos) {
            Reclassify(pos, keyEnd, kLanguageTokenClass::kConfigKey);
        }
        pos = SkipBlanks(sep + 1, commentStart);
    }
    ClassifyValue(pos, commentStart);
}

void YAMLLinePass::ClassifyValue(int pos, int end) {
    pos = SkipProperties(pos, end);
    if (pos >= end) {
        return;
    }
    if (((buffer[pos] == U'{') || (buffer[pos] == U'[')) && IsOperatorAt(pos)) {
        ClassifyFlow(pos, end);
        return;
    }
    ClassifyNode(pos, TrimEnd(pos, end));
}

// A one-line flow collection: '{ a: 1, b: [x, true] }'. The brackets and commas cut it into pieces; a
// piece followed by ': ' is a key, every other piece is a value.
void YAMLLinePass::ClassifyFlow(int start, int end) {
    int pieceStart = -1;
    for (int pos = start; pos < end; pos++) {
        char32_t ch = buffer[pos];
        bool isOperator = IsOperatorAt(pos);
        if (isOperator && ((ch == U'{') || (ch == U'}') || (ch == U'[') || (ch == U']') || (ch == U','))) {
            ClassifyFlowPiece(pieceStart, pos, false);
            pieceStart = -1;
            continue;
        }
        if (isOperator && (ch == U':') && (pieceStart >= 0)) {
            // In flow context a key may be followed directly by ',' / '}' as well as a blank
            bool isSeparator = (pos + 1 >= end) || IsBlank(buffer[pos + 1]) || (buffer[pos + 1] == U',') ||
                               (buffer[pos + 1] == U'}') || (buffer[pos + 1] == U']');
            if (isSeparator) {
                ClassifyFlowPiece(pieceStart, pos, true);
                pieceStart = -1;
                continue;
            }
        }
        if ((pieceStart < 0) && !IsBlank(ch)) {
            pieceStart = pos;
        }
    }
    ClassifyFlowPiece(pieceStart, end, false);
}

void YAMLLinePass::ClassifyFlowPiece(int start, int end, bool isKey) {
    if (start < 0) {
        return;
    }
    int pieceEnd = TrimEnd(start, end);
    if (pieceEnd <= start) {
        return;
    }
    if (isKey) {
        Reclassify(start, pieceEnd, kLanguageTokenClass::kConfigKey);
        return;
    }
    int pos = SkipProperties(start, pieceEnd);
    if (pos < pieceEnd) {
        ClassifyNode(pos, pieceEnd);
    }
}

// A single value: an alias, or a scalar typed only when it is the WHOLE value ('true story' stays text).
void YAMLLinePass::ClassifyNode(int pos, int end) {
    if ((pos >= end) || IsQuotedAt(pos)) {
        return;
    }
    if (buffer[pos] == U'*') {
        Reclassify(pos, TokenEnd(pos, end), kLanguageTokenClass::kMacroIdentifier);
        return;
    }
    auto cls = TypeOfScalar(buffer.substr(pos, end - pos));
    if (cls != kLanguageTokenClass::kRegular) {
        Reclassify(pos, end, cls);
    }
}

// Anchors '&name' and tags '!tag' / '!!str' in front of a value. Returns the position of the value.
int YAMLLinePass::SkipProperties(int pos, int end) {
    while ((pos < end) && ((buffer[pos] == U'&') || (buffer[pos] == U'!')) && !IsQuotedAt(pos)) {
        int tokenEnd = TokenEnd(pos, end);
        Reclassify(pos, tokenEnd, kLanguageTokenClass::kMacroIdentifier);
        pos = SkipBlanks(tokenEnd, end);
    }
    return pos;
}

// The ':' that ends a block key: an operator (not inside a string), followed by a blank or end of line. A
// flow collection starting first means there is no block key on this line.
int YAMLLinePass::FindKeySeparator(int start, int end) const {
    int n = static_cast<int>(buffer.size());
    for (int pos = start; pos < end; pos++) {
        if (!IsOperatorAt(pos)) {
            continue;
        }
        char32_t ch = buffer[pos];
        if ((ch == U'{') || (ch == U'[')) {
            return -1;
        }
        if ((ch == U':') && ((pos + 1 >= n) || IsBlank(buffer[pos + 1]))) {
            return pos;
        }
    }
    return -1;
}

int YAMLLinePass::SkipBlanks(int pos, int end) const {
    while ((pos < end) && IsBlank(buffer[pos])) {
        pos++;
    }
    return pos;
}

int YAMLLinePass::TrimEnd(int start, int end) const {
    while ((end > start) && IsBlank(buffer[end - 1])) {
        end--;
    }
    return end;
}

int YAMLLinePass::TokenEnd(int pos, int end) const {
    while ((pos < end) && !IsBlank(buffer[pos])) {
        pos++;
    }
    return pos;
}

// Was the char at 'pos' an operator to the tokenizer (so not inside a string or comment)?
bool YAMLLinePass::IsOperatorAt(int pos) const {
    return LineAttribEdit::ClassAt(tokenSpans, pos) == kLanguageTokenClass::kOperator;
}

bool YAMLLinePass::IsQuotedAt(int pos) const {
    return LineAttribEdit::ClassAt(tokenSpans, pos) == kLanguageTokenClass::kString;
}

void YAMLLinePass::Reclassify(int start, int end, kLanguageTokenClass cls) {
    if (end > start) {
        LineAttribEdit::ReclassifyRange(line, start, end, cls);
    }
}

bool YAMLLinePass::IsBlank(char32_t ch) {
    return (ch == U' ') || (ch == U'\t');
}

// true/false/null and their YAML 1.1 spellings (the editor's own config uses yes/no) -> keyword; a number
// -> number; anything else stays plain text.
kLanguageTokenClass YAMLLinePass::TypeOfScalar(const std::u32string &text) {
    static const std::vector<std::u32string> constants = {
        U"true", U"false", U"yes", U"no", U"on", U"off", U"null", U"~"
    };
    std::u32string lower = text;
    for (auto &ch : lower) {
        if ((ch >= U'A') && (ch <= U'Z')) {
            ch = ch - U'A' + U'a';
        }
    }
    for (auto &constant : constants) {
        if (lower == constant) {
            return kLanguageTokenClass::kKeyword;
        }
    }
    if (IsNumber(lower)) {
        return kLanguageTokenClass::kNumber;
    }
    return kLanguageTokenClass::kRegular;
}

// YAML numbers: [+-] decimal / float with exponent, 0x / 0o / 0b, .inf, .nan. 'text' is lower case.
// '1.2.3' (a version) and '3dfx' are not numbers.
bool YAMLLinePass::IsNumber(const std::u32string &text) {
    size_t i = 0;
    bool isSigned = !text.empty() && ((text[0] == U'+') || (text[0] == U'-'));
    if (isSigned) {
        i++;
    }
    auto rest = text.substr(i);
    if (rest.empty()) {
        return false;
    }
    if (rest == U".inf") {
        return true;
    }
    if (rest == U".nan") {
        return !isSigned;
    }

    if (!isSigned && (rest.size() > 2) && (rest[0] == U'0')) {
        int base = (rest[1] == U'x') ? 16 : (rest[1] == U'o') ? 8 : (rest[1] == U'b') ? 2 : 0;
        if (base != 0) {
            for (size_t k = 2; k < rest.size(); k++) {
                if (!IsDigitInBase(rest[k], base) && (rest[k] != U'_')) {
                    return false;
                }
            }
            return true;
        }
    }

    // [digits] [. [digits]] [e [+-] digits], at least one digit before the exponent
    size_t k = 0;
    size_t numDigits = 0;
    while ((k < rest.size()) && IsDigitInBase(rest[k], 10)) {
        k++;
        numDigits++;
    }
    if ((k < rest.size()) && (rest[k] == U'.')) {
        k++;
        while ((k < rest.size()) && IsDigitInBase(rest[k], 10)) {
            k++;
            numDigits++;
        }
    }
    if (numDigits == 0) {
        return false;
    }
    if ((k < rest.size()) && (rest[k] == U'e')) {
        k++;
        if ((k < rest.size()) && ((rest[k] == U'+') || (rest[k] == U'-'))) {
            k++;
        }
        size_t numExpDigits = 0;
        while ((k < rest.size()) && IsDigitInBase(rest[k], 10)) {
            k++;
            numExpDigits++;
        }
        if (numExpDigits == 0) {
            return false;
        }
    }
    return (k == rest.size());
}

bool YAMLLinePass::IsDigitInBase(char32_t ch, int base) {
    if (base == 16) {
        return ((ch >= U'0') && (ch <= U'9')) || ((ch >= U'a') && (ch <= U'f'));
    }
    return (ch >= U'0') && (ch < (U'0' + base));
}

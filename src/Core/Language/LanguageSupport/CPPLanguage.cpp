//
// Created by gnilk on 29.01.23.
//

//
// TODO: Number prefix handling
//

#include <string>

#include "CPPLanguage.h"
#include "CPPNumberMatcher.h"
#include "Core/UnicodeHelper.h"
using namespace gedit;

//
// state: main (and probably a few others)
static const std::u32string cppTypes = U"bool char char8_t char16_t char32_t double float float16_t float32_t float64_t float128_t bfloat16_t int long short signed unsigned void wchar_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t size_t ssize_t ptrdiff_t intptr_t uintptr_t intmax_t uintmax_t nullptr_t";

// see: https://en.cppreference.com/w/cpp/keyword
// Perhaps break this up a bit - like C++20, and so forth..
static const std::u32string cppKeywords = U"alignas alignof and and_eq asm auto bitand bitor break case catch class compl concept const consteval constexpr constinit const_cast continue co_await co_return co_yield decltype default delete do dynamic_cast else enum explicit export extern false final for friend goto if import inline module mutable namespace new noexcept not not_eq nullptr operator or or_eq override private protected public register reinterpret_cast requires return sizeof static static_assert static_cast struct switch template this thread_local throw true try typedef typeid typename union using virtual volatile while xor xor_eq";

// see: https://en.cppreference.com/w/cpp/language/punctuators
// Note: longer operators must come first — 3-char, then 2-char, then 1-char
static const std::u32string cppOperators = U"... <=> ->* <<= >>= == ++ -- << >> <= >= != += -= *= /= %= &= ^= |= -> :: && || .* : | % ~ ^ & ? ! . = + - < > ( , * / ) [ ] ; ' \"";
// The full operator set is used to identify post-fix operators but not used for classification
static const std::u32string cppOperatorsFull = U"... <=> ->* <<= >>= == ++ -- << >> <= >= != += -= *= /= %= &= ^= |= -> :: && || .* : | % ~ ^ & ? ! . = + - < > ( , * / ) [ ] ; ' \"";
//static const std::u32string cppOperatorsFull = U"== ++ -- << >> <= >= != += -= *= /= &= ^= |= :: /* */ // -> && || ^ & ? : ! . = + - < > ( , * / ) [ ] < > ; ' { } \" '";
static const std::u32string cppLineComment = U"//";
static const std::u32string cppCodeBlockStart = U"{";
static const std::u32string cppCodeBlockEnd = U"}";

// state: main -> '#' enters in_preprocessor
static const std::u32string cppPreProcStart = U"#";
// state: in_preprocessor - directive keywords following the '#'
static const std::u32string cppPreProcDirectives = U"include define undef ifdef ifndef if elif else endif pragma error warning line";

// state: main & in_block_comment
static const std::u32string cppBlockCommentStart = U"/* */";        // why do I have 'end block' here????
static const std::u32string cppBlockCommentStop = U"*/";
// state: in_string
static const std::u32string inStringOperators = UR"_(\" \\ ")_"; // not sure how to declare U32 raw strings?
static const std::u32string inStringPostFixOp = U"\"";

// state: in_char
static const std::u32string inCharOperators = UR"_(\\ \' ')_";
static const std::u32string inCharPostFixOp = U"'";

//
// Configure the tokenizer for C++
// NOTE: This can probably be driven 100% from a configuration file - it just set's up stuff...
//
bool CPPLanguage::Initialize() {
    auto state = tokenizer.GetOrAddState("main");
    state->SetIdentifiers(kLanguageTokenClass::kOperator, cppOperators);
    state->SetIdentifiers(kLanguageTokenClass::kKeyword, true, cppKeywords);
    state->SetIdentifiers(kLanguageTokenClass::kKnownType, true, cppTypes);
    state->SetIdentifiers(kLanguageTokenClass::kLineComment, cppLineComment);
    state->SetIdentifiers(kLanguageTokenClass::kBlockComment, cppBlockCommentStart);
    state->SetIdentifiers(kLanguageTokenClass::kCodeBlockStart, cppCodeBlockStart);
    state->SetIdentifiers(kLanguageTokenClass::kCodeBlockEnd, cppCodeBlockEnd);
    state->SetIdentifiers(kLanguageTokenClass::kPreProcessor, cppPreProcStart);
    state->SetPostFixIdentifiers(cppOperatorsFull);
    state->SetNumberMatcher(CPPNumberMatcher::Create());

    state->GetOrAddAction(U"\"", LangLineTokenizer::kAction::kPushState, "in_string");
    state->GetOrAddAction(U"/*", LangLineTokenizer::kAction::kPushState, "in_block_comment");
    state->GetOrAddAction(U"//", LangLineTokenizer::kAction::kPushState, "in_line_comment");
    state->GetOrAddAction(U"\'", LangLineTokenizer::kAction::kPushState, "in_char");
    state->GetOrAddAction(U"#", LangLineTokenizer::kAction::kPushState, "in_preprocessor");

    auto stateChr = tokenizer.GetOrAddState("in_char");
    stateChr->SetRegularTokenClass(kLanguageTokenClass::kChar);
    //auto u32inchrOp = UnicodeHelper::utf8to32(inCharOperators);
    stateChr->SetIdentifiers(kLanguageTokenClass::kChar, inCharOperators);
    stateChr->SetPostFixIdentifiers(inCharPostFixOp);
    stateChr->GetOrAddAction(U"'",LangLineTokenizer::kAction::kPopState);


    auto stateStr = tokenizer.GetOrAddState("in_string");
    stateStr->SetRegularTokenClass(kLanguageTokenClass::kString);
    //auto u32instrOp = UnicodeHelper::utf8to32(inStringOperators);
    stateStr->SetIdentifiers(kLanguageTokenClass::kString, inStringOperators);
    stateStr->SetPostFixIdentifiers(inStringPostFixOp);
    stateStr->GetOrAddAction(U"\"",LangLineTokenizer::kAction::kPopState);

    auto stateBlkComment = tokenizer.GetOrAddState("in_block_comment");
    // just testing, kFunky should be reclassified to 'kBlockComment' once this state is popped...
    stateBlkComment->SetIdentifiers(kLanguageTokenClass::kBlockComment, cppBlockCommentStop);
    stateBlkComment->SetPostFixIdentifiers(cppBlockCommentStop);
    stateBlkComment->GetOrAddAction(U"*/",LangLineTokenizer::kAction::kPopState);
    stateBlkComment->SetRegularTokenClass(kLanguageTokenClass::kCommentedText);

    // a line comment run's to new-line...
    auto stateLineComment = tokenizer.GetOrAddState("in_line_comment");
    stateLineComment->SetRegularTokenClass(kLanguageTokenClass::kCommentedText);
    stateLineComment->SetEOLAction(LangLineTokenizer::kAction::kPopState);

    // in_preprocessor: entered on '#'. Classifies the directive keyword and delegates #include to
    // the in_include sub-state. Operands (macro names/values) fall through as regular text.
    auto statePreProc = tokenizer.GetOrAddState("in_preprocessor");
    statePreProc->SetRegularTokenClass(kLanguageTokenClass::kRegular);
    statePreProc->SetIdentifiers(kLanguageTokenClass::kPreProcessor, true, cppPreProcDirectives);
    // No postfix identifiers: operands (pragma args, define bodies) are collected greedily to
    // whitespace. A postfix set including '"' or '(' here would yield empty tokens on directives
    // like #pragma GCC diagnostic ignored "..." and stop tokenization mid-line. The directive
    // keyword still breaks on whitespace, so spaced '#include <...>' still delegates to in_include.
    statePreProc->GetOrAddAction(U"include", LangLineTokenizer::kAction::kPushState, "in_include");
    statePreProc->GetOrAddAction(U"ifdef", LangLineTokenizer::kAction::kPushState, "in_pp_macro");
    statePreProc->GetOrAddAction(U"ifndef", LangLineTokenizer::kAction::kPushState, "in_pp_macro");
    statePreProc->GetOrAddAction(U"undef", LangLineTokenizer::kAction::kPushState, "in_pp_macro");
    statePreProc->SetEOLAction(LangLineTokenizer::kAction::kPopState);

    // in_pp_macro: sub-state of in_preprocessor for #ifdef/#ifndef/#undef - the operand is a single
    // macro name. No postfix identifiers: the name is collected greedily to whitespace so a bare
    // operator can't yield an empty token (which would skip the EOL flush and leak the state).
    auto statePPMacro = tokenizer.GetOrAddState("in_pp_macro");
    statePPMacro->SetRegularTokenClass(kLanguageTokenClass::kMacroIdentifier);
    statePPMacro->SetEOLAction(LangLineTokenizer::kAction::kPopState);

    // in_include: sub-state of in_preprocessor, handles the path following #include — delegates to in_string or in_include_angle
    auto stateInclude = tokenizer.GetOrAddState("in_include");
    stateInclude->SetRegularTokenClass(kLanguageTokenClass::kImport);
    stateInclude->SetIdentifiers(kLanguageTokenClass::kOperator, U"< \"");
    stateInclude->GetOrAddAction(U"\"", LangLineTokenizer::kAction::kPushState, "in_string");
    stateInclude->GetOrAddAction(U"<", LangLineTokenizer::kAction::kPushState, "in_include_angle");
    stateInclude->SetEOLAction(LangLineTokenizer::kAction::kPopState);

    // in_include_angle: content between < and > classified as kString
    auto stateIncludeAngle = tokenizer.GetOrAddState("in_include_angle");
    stateIncludeAngle->SetRegularTokenClass(kLanguageTokenClass::kString);
    stateIncludeAngle->SetIdentifiers(kLanguageTokenClass::kOperator, U">");
    stateIncludeAngle->SetPostFixIdentifiers(U">");
    stateIncludeAngle->GetOrAddAction(U">", LangLineTokenizer::kAction::kPopState);
    stateIncludeAngle->SetEOLAction(LangLineTokenizer::kAction::kPopState);


    tokenizer.SetStartState("main");
    // Register with the configuration

    // Grab configuration nodes
    ConfigFromNodeName("cpp");
    return true;
}

LanguageBase::kInsertAction CPPLanguage::OnPreCreateNewLine(const Line::Ref newLine) {
    if (newLine->Last() != U'}') {
        return kInsertAction::kDefault;
    }
    return kInsertAction::kNewLine;
}

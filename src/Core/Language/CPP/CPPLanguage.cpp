//
// Created by gnilk on 29.01.23.
//

#include <string>

#include "CPPLanguage.h"



// state: main (and probably a few others)
static const std::string cppTypes = "void int char";
static const std::string cppKeywords = "auto typedef class struct static enum for while if return const";
// FIXME: Remove '{' and '}' from here - add separatley with own identifiers, this way we can parse the meta data and
//        count block-start/ends in order to properly compute indentation...
// Note: Multi char operators must be declared first...
static const std::string cppOperators = "== ++ -- << >> += -= *= /= = + - < > ( , * ) [ ] < > ; ' \"";
static const std::string cppLineComment = "//";
static const std::string cppCodeBlockStart = "{";
static const std::string cppCodeBlockEnd = "}";
// state: main & in_block_comment
static const std::string cppBlockCommentStart = "/* */";
static const std::string cppBlockCommentStop = "*/";
// state: in_string
static const std::string inStringOperators = R"_(\" \\ ")_";
static const std::string inStringPostFixOp = "\"";

//
// Configure the tokenizer for C++
// NOTE: This can probably be driven 100% from a configuration file - it just set's up stuff...
//
bool CPPLanguage::Initialize() {
    auto state = tokenizer.GetOrAddState("main");
    state->SetIdentifiers(kLanguageTokenClass::kOperator, cppOperators.c_str());
    state->SetIdentifiers(kLanguageTokenClass::kKeyword, cppKeywords.c_str());
    state->SetIdentifiers(kLanguageTokenClass::kKnownType, cppTypes.c_str());
    state->SetIdentifiers(kLanguageTokenClass::kLineComment, cppLineComment.c_str());
    state->SetIdentifiers(kLanguageTokenClass::kBlockComment, cppBlockCommentStart.c_str());
    state->SetIdentifiers(kLanguageTokenClass::kCodeBlockStart, cppCodeBlockStart.c_str());
    state->SetIdentifiers(kLanguageTokenClass::kCodeBlockEnd, cppCodeBlockEnd.c_str());
    state->SetPostFixIdentifiers(cppOperators.c_str());

    state->GetOrAddAction("\"",gnilk::LangLineTokenizer::kAction::kPushState, "in_string");
    state->GetOrAddAction("/*", gnilk::LangLineTokenizer::kAction::kPushState, "in_block_comment");
    state->GetOrAddAction("//", gnilk::LangLineTokenizer::kAction::kPushState, "in_line_comment");

    auto stateStr = tokenizer.GetOrAddState("in_string");
    stateStr->SetRegularTokenClass(kLanguageTokenClass::kString);
    stateStr->SetIdentifiers(kLanguageTokenClass::kString, inStringOperators.c_str());
    stateStr->SetPostFixIdentifiers(inStringPostFixOp.c_str());
    stateStr->GetOrAddAction("\"", gnilk::LangLineTokenizer::kAction::kPopState);

    auto stateBlkComment = tokenizer.GetOrAddState("in_block_comment");
    // just testing, kFunky should be reclassified to 'kBlockComment' once this state is popped...
    stateBlkComment->SetIdentifiers(kLanguageTokenClass::kBlockComment, cppBlockCommentStop.c_str());
    stateBlkComment->SetPostFixIdentifiers(cppBlockCommentStop.c_str());
    stateBlkComment->GetOrAddAction("*/", gnilk::LangLineTokenizer::kAction::kPopState);
    stateBlkComment->SetRegularTokenClass(kLanguageTokenClass::kCommentedText);

    // a line comment run's to new-line...
    auto stateLineComment = tokenizer.GetOrAddState("in_line_comment");
    stateLineComment->SetRegularTokenClass(kLanguageTokenClass::kCommentedText);
    stateLineComment->SetEOLAction(gnilk::LangLineTokenizer::kAction::kPopState);


    tokenizer.SetStartState("main");

    // Register with the configuration

    return true;
}

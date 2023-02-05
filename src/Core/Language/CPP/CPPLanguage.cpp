//
// Created by gnilk on 29.01.23.
//

#include <string>

#include "CPPLanguage.h"

// state: main (and probably a few others)
static const std::string cppTypes = "void int char";
static const std::string cppKeywords = "auto typedef class struct static enum for while if return const";
// Note: Multi char operators must be declared first...
static const std::string cppOperators = "== ++ -- << >> += -= *= /= = + - < > ( , * ) { [ ] } < > ; ' \"";
static const std::string cppLineComment = "//";
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
    state->SetIdentifiers(gnilk::kLanguageTokenClass::kOperator, cppOperators.c_str());
    state->SetIdentifiers(gnilk::kLanguageTokenClass::kKeyword, cppKeywords.c_str());
    state->SetIdentifiers(gnilk::kLanguageTokenClass::kKnownType, cppTypes.c_str());
    state->SetIdentifiers(gnilk::kLanguageTokenClass::kLineComment, cppLineComment.c_str());
    state->SetIdentifiers(gnilk::kLanguageTokenClass::kBlockComment, cppBlockCommentStart.c_str());
    state->SetPostFixIdentifiers(cppOperators.c_str());

    state->GetOrAddAction("\"",gnilk::LangLineTokenizer::kAction::kPushState, "in_string");
    state->GetOrAddAction("/*", gnilk::LangLineTokenizer::kAction::kPushState, "in_block_comment");
    state->GetOrAddAction("//", gnilk::LangLineTokenizer::kAction::kPushState, "in_line_comment");

    auto stateStr = tokenizer.GetOrAddState("in_string");
    stateStr->SetRegularTokenClass(gnilk::kLanguageTokenClass::kString);
    stateStr->SetIdentifiers(gnilk::kLanguageTokenClass::kString, inStringOperators.c_str());
    stateStr->SetPostFixIdentifiers(inStringPostFixOp.c_str());
    stateStr->GetOrAddAction("\"", gnilk::LangLineTokenizer::kAction::kPopState);

    auto stateBlkComment = tokenizer.GetOrAddState("in_block_comment");
    // just testing, kFunky should be reclassified to 'kBlockComment' once this state is popped...
    stateBlkComment->SetIdentifiers(gnilk::kLanguageTokenClass::kBlockComment, cppBlockCommentStop.c_str());
    stateBlkComment->SetPostFixIdentifiers(cppBlockCommentStop.c_str());
    stateBlkComment->GetOrAddAction("*/", gnilk::LangLineTokenizer::kAction::kPopState);
    stateBlkComment->SetRegularTokenClass(gnilk::kLanguageTokenClass::kCommentedText);

    // a line comment run's to new-line...
    auto stateLineComment = tokenizer.GetOrAddState("in_line_comment");
    stateLineComment->SetRegularTokenClass(gnilk::kLanguageTokenClass::kCommentedText);
    stateLineComment->SetEOLAction(gnilk::LangLineTokenizer::kAction::kPopState);


    tokenizer.SetStartState("main");

    // Register with the configuration

    return true;
}

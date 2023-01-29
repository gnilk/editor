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
//
bool CPPLanguage::Initialize() {
    auto state = tokenizer.GetOrAddState("main");
    state->SetIdentifiers(gnilk::LangLineTokenizer::kOperator, cppOperators.c_str());
    state->SetIdentifiers(gnilk::LangLineTokenizer::kKeyword, cppKeywords.c_str());
    state->SetIdentifiers(gnilk::LangLineTokenizer::kKnownType, cppTypes.c_str());
    state->SetIdentifiers(gnilk::LangLineTokenizer::kLineComment, cppLineComment.c_str());
    state->SetIdentifiers(gnilk::LangLineTokenizer::kBlockComment, cppBlockCommentStart.c_str());
    state->SetPostFixIdentifiers(cppOperators.c_str());

    state->GetOrAddAction("\"",gnilk::LangLineTokenizer::kAction::kPushState, "in_string");
    state->GetOrAddAction("/*", gnilk::LangLineTokenizer::kAction::kPushState, "in_block_comment");

    auto stateStr = tokenizer.GetOrAddState("in_string");
    stateStr->SetIdentifiers(gnilk::LangLineTokenizer::kFunky, inStringOperators.c_str());
    stateStr->SetPostFixIdentifiers(inStringPostFixOp.c_str());
    stateStr->GetOrAddAction("\"", gnilk::LangLineTokenizer::kAction::kPopState);

    auto stateBlkComment = tokenizer.GetOrAddState("in_block_comment");
    stateBlkComment->SetIdentifiers(gnilk::LangLineTokenizer::kFunky, cppBlockCommentStop.c_str());
    stateBlkComment->SetPostFixIdentifiers(cppBlockCommentStop.c_str());
    stateBlkComment->GetOrAddAction("*/", gnilk::LangLineTokenizer::kAction::kPopState);

    tokenizer.PushState("main");

    return true;
}

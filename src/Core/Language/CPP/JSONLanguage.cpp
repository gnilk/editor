//
// Created by gnilk on 27.04.23.
//

#include "JSONLanguage.h"

using namespace gedit;

static const std::string jsonOperatorsFull = ", : [ ] { } \" \'";

static const std::string jsonBlockStart = "{";
static const std::string jsonBlockEnd = "}";

static const std::string inStringOp = "\"";
static const std::string inStringPostFixOp = "\"";

bool JSONLanguage::Initialize() {

    auto state = tokenizer.GetOrAddState("main");
    state->SetIdentifiers(kLanguageTokenClass::kOperator, jsonOperatorsFull.c_str());
    state->SetIdentifiers(kLanguageTokenClass::kCodeBlockStart, jsonBlockStart.c_str());
    state->SetIdentifiers(kLanguageTokenClass::kCodeBlockEnd, jsonBlockEnd.c_str());
//    state->SetPostFixIdentifiers(jsonOperatorsFull.c_str());

    state->GetOrAddAction("\"",LangLineTokenizer::kAction::kPushState, "in_string");

    auto stateStr = tokenizer.GetOrAddState("in_string");
    stateStr->SetRegularTokenClass(kLanguageTokenClass::kString);
    stateStr->SetIdentifiers(kLanguageTokenClass::kString, inStringOp.c_str());
    stateStr->SetPostFixIdentifiers(inStringPostFixOp.c_str());
    stateStr->GetOrAddAction("\"",LangLineTokenizer::kAction::kPopState);

    tokenizer.SetStartState("main");

    return true;
}

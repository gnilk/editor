//
// Created by gnilk on 27.04.23.
//

#include "JSONLanguage.h"
#include "Core/EditorConfig.h"

using namespace gedit;
//
// This is JS (JSON is also obviously supported - but will have a few keywords standing out)
//

static const std::string jsonOperatorsFull = "=== ++ -- == = < > ; . , : [ ] { } \' ( ) \"";

// Well, not quite right - but until we have an 'array' classification we use this...
static const std::string jsonObjectStart = "{";
static const std::string jsonObjectEnd = "}";

static const std::string jsonArrayStart = "[";
static const std::string jsonArrayEnd = "]";

static const std::string inStringOp = "\"";
static const std::string inStringPostFixOp = "\"";

static const std::string jsonKeywords = "true false null function for var let const";


bool JSONLanguage::Initialize() {

    auto state = tokenizer.GetOrAddState("main");
    state->SetIdentifiers(kLanguageTokenClass::kOperator, jsonOperatorsFull.c_str());
    state->SetIdentifiers(kLanguageTokenClass::kKeyword, jsonKeywords.c_str());
    state->SetIdentifiers(kLanguageTokenClass::kCodeBlockStart, jsonObjectStart.c_str());
    state->SetIdentifiers(kLanguageTokenClass::kCodeBlockEnd, jsonObjectEnd.c_str());
    state->SetIdentifiers(kLanguageTokenClass::kArrayStart, jsonArrayStart.c_str());
    state->SetIdentifiers(kLanguageTokenClass::kArrayEnd, jsonArrayEnd.c_str());
    state->SetPostFixIdentifiers(jsonOperatorsFull.c_str());

    state->GetOrAddAction("\"",LangLineTokenizer::kAction::kPushState, "in_string");

    auto stateStr = tokenizer.GetOrAddState("in_string");
    stateStr->SetRegularTokenClass(kLanguageTokenClass::kString);
    stateStr->SetIdentifiers(kLanguageTokenClass::kString, inStringOp.c_str());
    stateStr->SetPostFixIdentifiers(inStringPostFixOp.c_str());
    stateStr->GetOrAddAction("\"",LangLineTokenizer::kAction::kPopState);

    tokenizer.SetStartState("main");

    return true;
}

LanguageBase::kInsertAction JSONLanguage::OnPreInsertChar(Cursor &cursor, Line::Ref line, int ch) {
    // FIXME: This needs much more logic...
    if (ch == '}') {
        // FIXME: Check if line is 'empty' up-to x-pos
        cursor.position.x -= EditorConfig::Instance().tabSize;
        if (cursor.position.x < 0) {
            cursor.position.x = 0;
        }
    } else if (ch == ']') {
        // FIXME: Check if line is 'empty' up-to x-pos
        cursor.position.x -= EditorConfig::Instance().tabSize;
        if (cursor.position.x < 0) {
            cursor.position.x = 0;
        }
    }
    return kInsertAction::kDefault;
}

void JSONLanguage::OnPostInsertChar(Cursor &cursor, Line::Ref line, int ch) {
    if (ch == '{') {
        // FIXME: Check if chars to right are whitespace...
        line->Insert(cursor.position.x, '}');
    } else if (ch == '[') {
        // FIXME: Check if chars to right are whitespace...
        line->Insert(cursor.position.x, ']');
    }
}


//
// Created by gnilk on 27.04.23.
//

#include "JSONLanguage.h"

using namespace gedit;
//
// This is JS (JSON is also obviously supported - but will have a few keywords standing out)
//

static const std::u32string jsonOperatorsFull = U"=== ++ -- == = < > ; . , : [ ] { } \' ( ) \"";

// Well, not quite right - but until we have an 'array' classification we use this...
static const std::u32string jsonObjectStart = U"{";
static const std::u32string jsonObjectEnd = U"}";

static const std::u32string jsonArrayStart = U"[";
static const std::u32string jsonArrayEnd = U"]";

static const std::u32string inStringOp = U"\"";
static const std::u32string inStringPostFixOp = U"\"";

static const std::u32string jsLineComment = U"//";

static const std::u32string jsonKeywords = U"true false null function for var let const";


bool JSONLanguage::Initialize() {

    auto state = tokenizer.GetOrAddState("main");
    state->SetIdentifiers(kLanguageTokenClass::kOperator, jsonOperatorsFull);
    state->SetIdentifiers(kLanguageTokenClass::kKeyword, jsonKeywords);
    state->SetIdentifiers(kLanguageTokenClass::kCodeBlockStart, jsonObjectStart);
    state->SetIdentifiers(kLanguageTokenClass::kCodeBlockEnd, jsonObjectEnd);
    state->SetIdentifiers(kLanguageTokenClass::kArrayStart, jsonArrayStart);
    state->SetIdentifiers(kLanguageTokenClass::kArrayEnd, jsonArrayEnd);
    state->SetIdentifiers(kLanguageTokenClass::kLineComment, jsLineComment);
    state->SetPostFixIdentifiers(jsonOperatorsFull);

    state->GetOrAddAction(U"//",LangLineTokenizer::kAction::kPushState, "in_line_comment");
    state->GetOrAddAction(U"\"",LangLineTokenizer::kAction::kPushState, "in_string");

    auto stateStr = tokenizer.GetOrAddState("in_string");
    stateStr->SetRegularTokenClass(kLanguageTokenClass::kString);
    stateStr->SetIdentifiers(kLanguageTokenClass::kString, inStringOp);
    stateStr->SetPostFixIdentifiers(inStringPostFixOp);
    stateStr->GetOrAddAction(U"\"",LangLineTokenizer::kAction::kPopState);


    // a line comment run's to new-line...
    auto stateLineComment = tokenizer.GetOrAddState("in_line_comment");
    stateLineComment->SetRegularTokenClass(kLanguageTokenClass::kCommentedText);
    stateLineComment->SetEOLAction(LangLineTokenizer::kAction::kPopState);


    tokenizer.SetStartState("main");

    // Grab configuration (if any)
    ConfigFromNodeName("json");

    return true;
}

LanguageBase::kInsertAction JSONLanguage::OnPreInsertChar(Cursor &cursor, Line::Ref line, int ch) {
    // FIXME: This needs much more logic...
    if (ch == '}') {
        // FIXME: Check if line is 'empty' up-to x-pos
        cursor.position.x -= GetTabSize();
        if (cursor.position.x < 0) {
            cursor.position.x = 0;
        }
    } else if (ch == ']') {
        // FIXME: Check if line is 'empty' up-to x-pos
        cursor.position.x -= GetTabSize();
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


//
// Created by gnilk on 29.01.23.
//

//
// TODO: Number prefix handling
//

#include <string>

#include "CPPLanguage.h"
#include "Core/UnicodeHelper.h"
using namespace gedit;

// state: main (and probably a few others)
static const std::u32string cppTypes = U"void int char";
static const std::u32string cppKeywords = U"auto typedef class struct static enum for while if return const";
// Note: Multi char operators must be declared first...
static const std::u32string cppOperators = U"== ++ -- << >> <= >= != += -= *= /= &= ^= |= -> && || :: ^ & ? : ! . = + - < > ( , * / ) [ ] < > ; ' \"";
// The full operator set is used to identify post-fix operators but not used for classification..
// missing: % and %=
static const std::u32string cppOperatorsFull = U"== ++ -- << >> <= >= != += -= *= /= &= ^= |= :: /* */ // -> && || ^ & ? : ! . = + - < > ( , * / ) [ ] < > ; ' { } \" \'";
static const std::u32string cppLineComment = U"//";
static const std::u32string cppCodeBlockStart = U"{";
static const std::u32string cppCodeBlockEnd = U"}";
// state: main & in_block_comment
static const std::u32string cppBlockCommentStart = U"/* */";
static const std::u32string cppBlockCommentStop = U"*/";
// state: in_string
static const std::string inStringOperators = R"_(\" \\ ")_"; // not sure how to declare U32 raw strings?
static const std::u32string inStringPostFixOp = U"\"";

//static const std::string inCharOperators = R"_(\" \\)_"; // not sure how to declare U32 raw strings?
static const std::u32string inCharOperators = U"\" \\ '";
static const std::u32string inCharPostFixOp = U"'";

//
// Configure the tokenizer for C++
// NOTE: This can probably be driven 100% from a configuration file - it just set's up stuff...
//
bool CPPLanguage::Initialize() {
    auto state = tokenizer.GetOrAddState("main");
    state->SetIdentifiers(kLanguageTokenClass::kOperator, cppOperators);
    state->SetIdentifiers(kLanguageTokenClass::kKeyword, cppKeywords);
    state->SetIdentifiers(kLanguageTokenClass::kKnownType, cppTypes);
    state->SetIdentifiers(kLanguageTokenClass::kLineComment, cppLineComment);
    state->SetIdentifiers(kLanguageTokenClass::kBlockComment, cppBlockCommentStart);
    state->SetIdentifiers(kLanguageTokenClass::kCodeBlockStart, cppCodeBlockStart);
    state->SetIdentifiers(kLanguageTokenClass::kCodeBlockEnd, cppCodeBlockEnd);
    state->SetPostFixIdentifiers(cppOperatorsFull);

    state->GetOrAddAction(U"\"", LangLineTokenizer::kAction::kPushState, "in_string");
    state->GetOrAddAction(U"/*", LangLineTokenizer::kAction::kPushState, "in_block_comment");
    state->GetOrAddAction(U"//", LangLineTokenizer::kAction::kPushState, "in_line_comment");
    state->GetOrAddAction(U"\'", LangLineTokenizer::kAction::kPushState, "in_char");

    auto stateChr = tokenizer.GetOrAddState("in_char");
    stateChr->SetRegularTokenClass(kLanguageTokenClass::kChar);
//    auto u32charOp = UnicodeHelper::utf8to32(inCharOperators);
//    stateChr->SetIdentifiers(kLanguageTokenClass::kChar, u32charOp);
    stateChr->SetIdentifiers(kLanguageTokenClass::kChar, inCharOperators);
    stateChr->SetPostFixIdentifiers(inCharPostFixOp);
    stateChr->GetOrAddAction(U"'",LangLineTokenizer::kAction::kPopState);


    auto stateStr = tokenizer.GetOrAddState("in_string");
    stateStr->SetRegularTokenClass(kLanguageTokenClass::kString);
    auto u32instrOp = UnicodeHelper::utf8to32(inStringOperators);
    stateStr->SetIdentifiers(kLanguageTokenClass::kString, u32instrOp);
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



LanguageBase::kInsertAction CPPLanguage::OnPreInsertChar(Cursor &cursor, Line::Ref line, char32_t ch) {
    // FIXME: This needs much more logic...
    if(ch == U'}') {
        // FIXME: Check if line is 'empty' up-to x-pos
        cursor.position.x -= GetTabSize();
        if (cursor.position.x < 0) {
            cursor.position.x = 0;
        }
    } else if (ch == U')') {
        if ((cursor.position.x == line->Length()-1) && (line->Last() == U')')) {
            // no insert - just skip over ')' and stop the insert
            cursor.position.x++;
            return kInsertAction::kNoInsert;
        }
    }
    return kInsertAction::kDefault;

}

void CPPLanguage::OnPostInsertChar(Cursor &cursor, Line::Ref line, char32_t ch) {
    if (ch == U'{') {
        // FIXME: Check if chars to right are whitespace...
        line->Insert(cursor.position.x, U'}');
    } else if (ch == '[') {
        // FIXME: Check if chars to right are whitespace...
        line->Insert(cursor.position.x, U']');
    } else if (ch == U'(') {
        // FIXME: Check if chars to right are whitespace...
        line->Insert(cursor.position.x, U')');
    }
}

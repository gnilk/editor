//
// Created by gnilk on 29.01.23.
//

#include <string>
#include <unordered_map>
#include <map>
#include "LangToken.h"
#include "LanguageTokenClass.h"

using namespace gnilk;

//
// This mapping is used to identify the color to be used.
// Currently several are mapped to the same (like operator is used multiple times)
// so lookup is currently...
//  tokenClass => name => color_index => color
//
static const std::unordered_map<kLanguageTokenClass, std::string> tokenNames = {
        {kLanguageTokenClass::kUnknown,"unknown"},
        {kLanguageTokenClass::kRegular,"regular"},
        {kLanguageTokenClass::kOperator,"operator"},
        {kLanguageTokenClass::kKeyword,"keyword"},
        {kLanguageTokenClass::kKnownType,"known_type"},
        // FIXME: Implement this => Require custom matching for identifiers => see note in "identifierlist"
        {kLanguageTokenClass::kNumber,"number"},
        {kLanguageTokenClass::kString,"string"},
        {kLanguageTokenClass::kLineComment,"line_comment"},
        {kLanguageTokenClass::kBlockComment,"block_comment"},
        {kLanguageTokenClass::kCommentedText,"commented_text"},
        {kLanguageTokenClass::kCodeBlockStart,"operator"},
        {kLanguageTokenClass::kCodeBlockEnd,"operator"},
        {kLanguageTokenClass::kFunky,"funky"},
};

bool gnilk::IsLanguageTokenClass(int num) {
    static constexpr int maxLangClass = static_cast<int>(kLanguageTokenClass::kLastTokenClass);
    if ((num < 0) || (num >=  maxLangClass)) {
        return false;
    }
    return true;
}

void LangToken::ToLineAttrib(std::vector<Line::LineAttrib> &outAttributes, std::vector<LangToken> &tokens) {
    outAttributes.clear();
    for(auto &t : tokens) {
        Line::LineAttrib attrib;
        attrib.idxOrigString = t.idxOrigStr;
        attrib.idxColor = static_cast<int>(t.classification);
        attrib.tokenClass = t.classification;
        outAttributes.push_back(attrib);
    }

}

// This is wrong, should be named "LanguageTokenClassToGroup"..
const std::string &gnilk::LanguageTokenClassToString(kLanguageTokenClass tokenClass) {
    auto it = tokenNames.find(tokenClass);
    if (it == tokenNames.end()) {
        printf("UNSUPPORTED TOKEN CLASS (%d), UPDATE THE ARRAY!!!!\n", tokenClass);
        exit(1);
    }
    return it->second;
}
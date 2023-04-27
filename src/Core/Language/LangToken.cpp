//
// Created by gnilk on 29.01.23.
//

#include <string>
#include <unordered_map>
#include <map>
#include "LangToken.h"
#include "LanguageTokenClass.h"

using namespace gedit;

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
        {kLanguageTokenClass::kArrayStart,"operator"},
        {kLanguageTokenClass::kArrayEnd,"operator"},
        {kLanguageTokenClass::kFunky,"funky"},
};

void LangToken::ToLineAttrib(std::vector<gedit::Line::LineAttrib> &outAttributes, std::vector<LangToken> &tokens) {
    outAttributes.clear();

    // The rendering don't require us to have 'attributes' for each line, inserting one by default
    // is perhaps not needed...  should re-think this...
    // Could also exit early if this is the case...
    // However, forcing attributes to be set will make the rendering logic a bit easier...

    // Attributes should start at '0' if not it means we have white space in the beginning
    // In that case, insert a 'regular'
    if ((tokens.size() == 0) || (tokens[0].idxOrigStr > 0)) {
        gedit::Line::LineAttrib attrib;
        attrib.idxOrigString = 0;
        attrib.tokenClass = kLanguageTokenClass::kRegular;
        outAttributes.push_back(attrib);
    }
    // Now do the rest...
    for(auto &t : tokens) {
        gedit::Line::LineAttrib attrib;
        attrib.idxOrigString = t.idxOrigStr;
        attrib.tokenClass = t.classification;
        outAttributes.push_back(attrib);
    }

}

namespace gedit {
    bool IsLanguageTokenClass(int num) {
        static constexpr int maxLangClass = static_cast<int>(kLanguageTokenClass::kLastTokenClass);
        if ((num < 0) || (num >=  maxLangClass)) {
            return false;
        }
        return true;
    }


    const std::string &LanguageTokenClassToString(kLanguageTokenClass tokenClass) {
        auto it = tokenNames.find(tokenClass);
        if (it == tokenNames.end()) {
            printf("UNSUPPORTED TOKEN CLASS (%d), UPDATE THE ARRAY!!!!\n", tokenClass);
            exit(1);
        }
        return it->second;
    }
}

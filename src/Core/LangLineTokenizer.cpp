/*-------------------------------------------------------------------------
File    : $Archive: LangLineTokenizer.cpp $
Author  : $Author: FKling $
Version : $Revision: 1 $
Orginal : 2009-10-17, 15:50
Descr   : Simple and extensible line tokenzier, pre-processes the data 
	      and stores tokens in a list.

Modified: $Date: $ by $Author: FKling $
---------------------------------------------------------------------------

\History
- 23.09.22, FKling, Multi char operators
- 14.03.14, FKling, published on github
- 25.10.09, FKling, Implementation

---------------------------------------------------------------------------*/

#include <stdlib.h>
#include <vector>
#include <string>
#include <string.h>

#include "LangLineTokenizer.h"

using namespace gnilk;

LangLineTokenizer::LangLineTokenizer(const char *sOperators, const char *sKeywords, const char *sTypes) {
    SplitToStringList(sOperators, operators);
    SplitToStringList(sKeywords, keywords);
    SplitToStringList(sTypes, knowntypes);

}

bool LangLineTokenizer::InStringList(std::vector<std::string> &strList, const char *input, int &outSz) {
    for (auto s: strList) {
        if (!strncmp(s.c_str(), input, s.size())) {
            outSz = s.size();
            return true;
        }
    }
    return false;
}

bool LangLineTokenizer::IsOperator(const char *input, int &outSzOperator) {
    return InStringList(operators, input, outSzOperator);
}
bool LangLineTokenizer::IsKeyword(const char *input, int &outSzKeyword) {
    return InStringList(keywords, input, outSzKeyword);
}
bool LangLineTokenizer::IsKnownType(const char *input, int &outSzType) {
    return InStringList(knowntypes, input, outSzType);
}

void LangLineTokenizer::SplitToStringList(const char *input, std::vector<std::string> &outList) {
    char tmp[256];
    char *parsepoint = (char *)input;

    if (input == nullptr) {
        return;
    }

    while (GetNextTokenNoOperator(tmp, 256, &parsepoint)) {
        outList.push_back(std::string(tmp));
    }
}


void LangLineTokenizer::PrepareTokens(std::vector<Token> &tokens, const char *input) {
    char tmp[256];
    char *parsepoint = (char *) input;

    // Reset current token
    // FIXME: It should be possible to set this
    currentTokenClass = kUnknown;

    while(true) {
        auto [ok, classification] = GetNextToken(tmp, 256, &parsepoint);
        if (!ok) {
            break;
        }

        int len = strlen(tmp);
        int pos = (parsepoint - input) - len;
        Token token { .string = std::string(tmp), .classification = classification, .idxOrigStr = pos };
        tokens.push_back(token);

        // Save this, has implications...
        currentTokenClass = classification;
    }
}

void LangLineTokenizer::PrepareTokens2(std::vector<Token> &tokens, const char *input) {
    char tmp[256];
    char *parsepoint = (char *) input;

    // Reset current token
    // FIXME: It should be possible to set this
    currentTokenClass = kUnknown;

    printf("CurrentState: %s\n", stateStack.top()->name.c_str());

    while(true) {
        auto currentState = stateStack.top();

        auto [ok, classification] = GetNextTokenNew(tmp, 256, &parsepoint);
        if (!ok) {
            break;
        }

        printf("s: %s, tok: %s\n", currentState->name.c_str(), tmp);

        int len = strlen(tmp);
        if (len == 0) {
            return;
        }
        int pos = (parsepoint - input) - len;

        if (currentState->HasActionForToken(tmp)) {
            auto action = currentState->GetAction(tmp);
            if (action->action == kAction::kPushState) {
                printf("PushState, %s -> %s (at token: %s)\n", currentState->name.c_str(), action->stateName.c_str(), tmp);
                PushState(action->stateName.c_str());
            } else if (action->action == kAction::kPopState) {

                auto oldState  = PopState();
                currentState = stateStack.top();

                printf("PopState, %s -> %s (at token: %s)\n", oldState->name.c_str(), stateStack.top()->name.c_str(), tmp);

                // When we pop, the token causing the pop belongs to the popped-context
                // As it is the tail end of the token causing the push in the first place...   <- read several times...

                // Rewrite classification...
                auto [ok, tokenClass] = currentState->ClassifyToken(tmp);
                if (ok) {
                    printf("  Reclassification: %d -> %d\n", classification, tokenClass);
                    classification = tokenClass;
                }
            }
        }

        // If this is regular text - reclassify it depending on the state (this allows for comments/string and other
        // encapsulation statements to override... (#include)
        if (classification == kTokenClass::kRegular) {
            classification = currentState->regularTokenClass;
        }

        Token token { .string = std::string(tmp), .classification = classification, .idxOrigStr = pos };
        tokens.push_back(token);

        // Save this, has implications...
        currentTokenClass = classification;
    }

}


//
// Perhaps split this to own CPP language handler, this way we can reuse..
//
//
// FIXME: need state handling and also (presumable) state handling between call's into the instance...
//        reasons:
//        - Keep C/CPP strings "\""  - strings have 'escape' operator
//        - /*   - is multi-line
//        - probably a lot more...
//
// - Basically
// - 1) There is an inital state (main - or whatever)
// - 2) Each state have their own set of 'operators', 'keywords', etc...
// - 3) Each operator (or group thereof) can push a new state
// - 4) Each push must have a rule to pop the state back to main..
//
// Example: main (rules should be more complex)
//          - name: block comment start
//            - state: main
//            - match: /*
//            - result: operator.block_comment_start
//            - action: push state.block_comment
//          - name: block comment end
//            - state: block_comment
//            - match: */
//            - result: operator.block_comment_end
//            - action: pop
//          - name: string operator
//            - state: main
//            - match: "
//            - result: operator.string
//            - action: push state.inside_c_string
//          - name: string end operator
//            - state: inside_c_string
//            - match: " but not \"
//            - action: pop
//          - name: raw_string
//            - state: main
//            - match: R"_<prefix>
//            - result: operator.raw_string
//            - action: push state.inside_cpp_rawstring
//          - name: raw_string_end_operator
//            - state: inside_cpp_rawstring
//            - match: <prefix>_"
//            - result: operator.raw_string
//            - action: push state.inside_cpp_rawstring
//
//
// If I adopt this way of dealing with it - I can reuse the language definitions from Sublime..
// Which obviously would save me quite a lot of time...
//
//
//
//
std::pair<bool, LangLineTokenizer::kTokenClass> LangLineTokenizer::GetNextTokenNew(char *dst, int nMax, char **input) {

    auto currentState = stateStack.top();

    if (!SkipWhiteSpace(input)) {
        return {false, kTokenClass::kRegular};
    }
    int szOperator = 0;
    if (currentState == nullptr) {
        fprintf(stderr,"ERR: state stack empty!\n");
        exit(1);
    }

    for(auto &kvp : currentState->identifiers) {
        if (kvp.second.IsMatch(*input, szOperator)) {
           strncpy(dst, *input, szOperator);
            (*input) += szOperator;
            // Not needed - but makes rules easier...
            dst[szOperator] = '\0';
            return {true, kvp.second.classification};
        }
    }

    // This is just to avoid clutter in the while-loop below
    int idxDst=0;
    auto chkPostFix= [currentState, input, &szOperator](char *dst)->bool {
        if (currentState == nullptr) {
            return false;
        }
        return currentState->postfixIdentifiers.IsMatch(*input, szOperator);
    };

    while ((**input != '\0') && !isspace(**input) && !chkPostFix(dst)) {
        dst[idxDst++] = **input;

        // This is a developer problem, ergo - safe to exit..
        if (idxDst >= nMax) {
            fprintf(stderr, "ERR: GetNextToken, token size larger than buffer (>nMax)\n");
            exit(1);
        }

        (*input)++;
    }
    dst[idxDst] = '\0';
    return {true, kRegular};

}

std::pair<bool, LangLineTokenizer::kTokenClass> LangLineTokenizer::GetNextToken(char *dst, int nMax, char **input) {

    if (!SkipWhiteSpace(input)) {
        return {false, kTokenClass::kRegular};
    }

    int szOperator = 0;
    kTokenClass classification = kRegular;

    int i = 0;
    if (IsOperator(*input, szOperator)) {
        //
        // need rules here, perhaps a classification stack???
        //

        strncpy(dst, *input, szOperator);
        (*input) += szOperator;
        i = szOperator;
        // Not needed - but makes rules easier...
        dst[szOperator] = '\0';

        // Here we have some kind of rule to detect '\"' inside a string...
        // can we generalize this? (Sublime has a quite complex scripting scheme for it - not sure I want to replicate
        // that...

        // We should direct this to some kind of rules engine..
        // since each language will have their own set of rules...
        if ((currentTokenClass == kString) && !strcmp(dst,"\\\"")) {
          classification = currentTokenClass;
        } else {
           if (!strcmp(dst, "\"")) {
                classification = kString;
            } else {
                classification = kOperator;
            }
        }
    } else if (IsKeyword(*input, szOperator)) {
        strncpy(dst, *input, szOperator);
        (*input) += szOperator;
        i = szOperator;
        classification = kKeyword;
    } else if (IsKnownType(*input, szOperator)) {
        strncpy(dst, *input, szOperator);
        (*input) += szOperator;
        i = szOperator;
        classification = kKnownType;
    } else {
        //while (!isspace(**input) && !IsOperator(*input, szOperator) && (**input != '\0')) {
        while (!isspace(**input) && !IsOperator(*input, szOperator) && (**input != '\0')) {
            dst[i++] = **input;

            // This is a developer problem, ergo - safe to exit..
            if (i >= nMax) {
                fprintf(stderr, "ERR: GetNextToken, token size larger than buffer (>nMax)\n");
                exit(1);
            }

            (*input)++;
        }
        if (currentTokenClass == kString) {
            classification = kString;
        }
    }

    dst[i] = '\0';
    return {true, classification};
}

char *LangLineTokenizer::GetNextTokenNoOperator(char *dst, int nMax, char **input) {
    if (!SkipWhiteSpace(input)) {
        return nullptr;
    }

    int i = 0;
    while (!isspace(**input) && (**input != '\0')) {
        dst[i++] = **input;

        // This is a developer problem, ergo - safe to exit..
        if (i >= nMax) {
            fprintf(stderr, "ERR: GetNextToken, token size larger than buffer (>nMax)\n");
            exit(1);
        }

        (*input)++;
    }
    dst[i] = '\0';
    return dst;
}

bool LangLineTokenizer::SkipWhiteSpace(char **input) {
    if (**input == '\0') {
        return false;
    }
    while ((isspace(**input)) && (**input != '\0')) {
        (*input)++;
    }
    if (**input == '\0') {
        return false;    // only trailing space
    }
    return true;
}



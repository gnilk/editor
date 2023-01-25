/*-------------------------------------------------------------------------
File    : $Archive: Tokenizer.cpp $
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

#include "Tokenizer.h"

using namespace gnilk;

Tokenizer::Tokenizer(const char *sInput, const char *sOperators) {
    iTokenIndex = 0;
    PrepareOperators(sOperators);
    PrepareTokens(sInput);
}

Tokenizer::Tokenizer(const char *sInput) {
    iTokenIndex = 0;
    PrepareOperators(" ");
    PrepareTokens(sInput);
}

bool Tokenizer::HasMore() const {
    return (iTokenIndex < tokens.size());

}

const char *Tokenizer::Next() {
    if (iTokenIndex > tokens.size()) return nullptr;
    return tokens[iTokenIndex++].c_str();
}

const char *Tokenizer::Previous() {
    if (iTokenIndex > 0) return tokens[--iTokenIndex].c_str();
    return nullptr;
}

const char *Tokenizer::Peek() const {
    if (iTokenIndex < tokens.size()) return tokens[iTokenIndex].c_str();
    return nullptr;
}

int Tokenizer::Case(const char *sValue, const char *sInput) {
    Tokenizer tokens(sInput);// = new Tokenizer(sInput);

    int idx = 0;
    while (tokens.HasMore()) {
        if (!strcmp(sValue, tokens.Next())) return idx;
        idx++;
    }
    return -1;
}


bool Tokenizer::IsOperator(const char *input, int &outSzOperator) {
    for (auto s: operators) {
        if (!strncmp(s.c_str(), input, s.size())) {
            outSzOperator = s.size();
            return true;
        }
    }
    return false;
}

void Tokenizer::PrepareOperators(const char *input) {
    char tmp[256];
    char *parsepoint = (char *)input;
    while (GetNextTokenNoOperator(tmp, 256, &parsepoint)) {
        operators.push_back(std::string(tmp));
    }
}


void Tokenizer::PrepareTokens(const char *input) {
    char tmp[256];
    char *parsepoint = (char *) input;
    while (GetNextToken(tmp, 256, &parsepoint)) {
        tokens.push_back(std::string(tmp));
    }
}

char *Tokenizer::GetNextToken(char *dst, int nMax, char **input) {

    if (!SkipWhiteSpace(input)) {
        return nullptr;
    }

    int szOperator = 0;

    int i = 0;
    if (IsOperator(*input, szOperator)) {
        strncpy(dst, *input, szOperator);
        (*input) += szOperator;
        i = szOperator;
    } else {
        while (!isspace(**input) && !IsOperator(*input, szOperator) && (**input != '\0')) {
            dst[i++] = **input;

            // This is a developer problem, ergo - safe to exit..
            if (i >= nMax) {
                fprintf(stderr, "ERR: GetNextToken, token size larger than buffer (>nMax)\n");
                exit(1);
            }


            (*input)++;
        }
    }
    dst[i] = '\0';
    return dst;
}

char *Tokenizer::GetNextTokenNoOperator(char *dst, int nMax, char **input) {
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

bool Tokenizer::SkipWhiteSpace(char **input) {
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



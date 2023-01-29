//
// Created by gnilk on 29.01.23.
//

#ifndef EDITOR_LANGUAGEBASE_H
#define EDITOR_LANGUAGEBASE_H

#include "LangLineTokenizer.h"

class LanguageBase {
public:
    LanguageBase() = default;
    virtual ~LanguageBase() = default;

    // Implement this and setup the tokenizer
    virtual bool Initialize() { return false; }; // You should really implement this...

    const gnilk::LangLineTokenizer &Tokenizer() { return  tokenizer; }
protected:
    gnilk::LangLineTokenizer tokenizer;
};


#endif //EDITOR_LANGUAGEBASE_H

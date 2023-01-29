//
// Created by gnilk on 29.01.23.
//

#ifndef EDITOR_CPPLANGUAGE_H
#define EDITOR_CPPLANGUAGE_H

#include "Core/Language/LanguageBase.h"


class CPPLanguage : public LanguageBase {
public:
    CPPLanguage() = default;
    virtual ~CPPLanguage() = default;

    bool Initialize() override;
private:

// declare in-string operators
};


#endif //EDITOR_CPPLANGUAGE_H

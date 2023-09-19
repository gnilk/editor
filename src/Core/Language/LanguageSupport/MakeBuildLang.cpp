//
// Created by gnilk on 18.09.23.
//

#include "MakeBuildLang.h"

using namespace gedit;

static const std::u32string makeKeywords = U"error";
static const std::u32string makeOperators = U"*** : ^";


bool MakeBuildLang::Initialize() {
    auto state = tokenizer.GetOrAddState("main");
    state->SetIdentifiers(kLanguageTokenClass::kOperator, makeOperators);
    state->SetIdentifiers(kLanguageTokenClass::kKeyword, makeKeywords);
    state->SetPostFixIdentifiers(makeOperators);

    tokenizer.SetStartState("main");
    ConfigFromNodeName("make");
}

void MakeBuildLang::OnPostProcessParsedLine(Line::Ref line) {

}

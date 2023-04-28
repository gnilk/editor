//
// Created by gnilk on 28.04.23.
//

#include "DefaultLanguage.h"

using namespace gedit;

bool DefaultLanguage::Initialize() {
    auto state = tokenizer.GetOrAddState("main");
    tokenizer.SetStartState("main");
    return true;
}

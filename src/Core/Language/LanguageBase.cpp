//
// Created by gnilk on 29.01.23.
//

// not much here - just a place-hold in case the base class needs implementation..
#include "LanguageBase.h"
#include "Core/Config/Config.h"
using namespace gedit;

int LanguageBase::GetTabSize() {
    auto tabSize = GetInt("tabsize",-1);
    if (tabSize < 0) {
        auto languages = Config::Instance().GetNode("languages");
        auto defLang = languages.GetNode("default");
        tabSize = defLang.GetInt("tabsize", 4);
    }
    return tabSize;
}

void LanguageBase::ConfigFromNodeName(const std::string nodeName) {
    if (!Config::Instance().HasKey("languages")) return;

    auto langNode = Config::Instance().GetNode("languages");
    if (!langNode.HasKey(nodeName)) return;

    auto defaultLang = langNode.GetNode(nodeName);

    CopyNode(defaultLang);

}

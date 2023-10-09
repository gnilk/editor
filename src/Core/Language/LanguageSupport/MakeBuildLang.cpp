//
// Created by gnilk on 18.09.23.
//

#include "Core/UnicodeHelper.h"
#include "MakeBuildLang.h"

using namespace gedit;

static const std::u32string makeKeywords = U"error wef";
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
    std::vector<Part> parts;
    auto callback = [&parts](const Line::LineAttribIterator &itAttrib, std::u32string &strOut) {
        Part part;
        part.attrib = *itAttrib;
        part.string = strOut;
        parts.push_back(part);
    };
    line->IterateWithAttributes(callback);

    for(size_t i=0;i<parts.size();i++) {
        auto &part = parts[i];
        if ((part.attrib.tokenClass == kLanguageTokenClass::kKeyword) && (part.string == U"error")) {
            if (parts.size() == 4) {
                int breakme = 1;
            }
            // have error - what do we do now..   =)
            printf("Build error detected (parts: %zu)\n", parts.size());
            printf("  file: %s\n", UnicodeHelper::utf32to8(parts[0].string).c_str());
            printf("  line: %s\n", UnicodeHelper::utf32to8(parts[2].string).c_str());
            printf("  row : %s\n", UnicodeHelper::utf32to8(parts[4].string).c_str());

            std::u32string msg(line->Buffer(), parts[8].attrib.idxOrigString);
            printf("  err : %s\n", UnicodeHelper::utf32to8(msg).c_str());

        }
    }

}

//
// Created by gnilk on 18.09.23.
//

#ifndef GOATEDIT_MAKEBUILDLANG_H
#define GOATEDIT_MAKEBUILDLANG_H

#include <memory>
#include "Core/Language/LanguageBase.h"

namespace gedit {
    // This is a test to see if I can use the language parser to parse the output from 'make' and similar..
    class MakeBuildLang : public LanguageBase {
    public:
        MakeBuildLang() = default;
        virtual ~MakeBuildLang() = default;

        static LanguageBase::Ref Create() {
            return std::make_shared<MakeBuildLang>();
        }

        bool Initialize() override;
        void OnPostProcessParsedLine(Line::Ref line) override;

        const std::u32string &Identifier() override {
            static std::u32string cppIdentifier = U"make";
            return cppIdentifier;
        }
    private:
        struct Part {
            Line::LineAttrib attrib;
            std::u32string string;
        };

    };
}


#endif //GOATEDIT_MAKEBUILDLANG_H

//
// A language from a file (docs/support-languages.md §3). Initialize() makes exactly the GetOrAddState /
// SetIdentifiers / GetOrAddAction / SetEOLAction calls a C++ language makes - there is no second tokenizer.
//
// Settings: the file's 'language:' section (its chain folded in), with config.yml 'languages.<name>' over it.
//

#ifndef GEDIT_DATALANGUAGE_H
#define GEDIT_DATALANGUAGE_H

#include "Core/Language/LanguageBase.h"
#include "Core/Language/Data/LanguageDefinition.h"

namespace gedit {
    class DataLanguage : public LanguageBase {
    public:
        using Ref = std::shared_ptr<DataLanguage>;
    public:
        explicit DataLanguage(LanguageDefinition newDefinition);
        virtual ~DataLanguage() = default;

        static Ref Create(LanguageDefinition definition) {
            return std::make_shared<DataLanguage>(std::move(definition));
        }

        bool Initialize() override;
        const std::u32string &Identifier() override { return identifier; }
        const std::u32string &GetLineComment() override { return definition.lineComment; }

        const LanguageDefinition &Definition() const { return definition; }

        // Where the file was loaded from - shown by '.ll'
        void SetOrigin(const std::string &newOrigin) { origin = newOrigin; }
        const std::string &GetOrigin() const { return origin; }

    private:
        void BuildState(const LanguageDefinition::State &stateDefinition);
        void ApplySettings();

    private:
        LanguageDefinition definition;
        std::u32string identifier;
        std::string origin;
    };
}

#endif //GEDIT_DATALANGUAGE_H

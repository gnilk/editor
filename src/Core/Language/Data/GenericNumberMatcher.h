//
// The number matcher a language file asks for with 'numbers: generic' (docs/support-languages.md §5): decimal with
// '_' separators, 0x / 0o / 0b, a fraction, an exponent, and one suffix character from the file ('j' for Python).
// A number glued to a word is no number ('64bit', '2nd') - the whole word stays regular. CPPNumberMatcher stays C++'s.
//

#ifndef GEDIT_GENERICNUMBERMATCHER_H
#define GEDIT_GENERICNUMBERMATCHER_H

#include <memory>
#include <string>

#include "Core/Language/NumberMatcherBase.h"

namespace gedit {
    class GenericNumberMatcher : public NumberMatcherBase {
    public:
        using Ref = std::shared_ptr<GenericNumberMatcher>;
    public:
        explicit GenericNumberMatcher(std::u32string newSuffixes) : suffixes(std::move(newSuffixes)) {
        }
        virtual ~GenericNumberMatcher() = default;

        static Ref Create(std::u32string suffixes = {}) {
            return std::make_shared<GenericNumberMatcher>(std::move(suffixes));
        }

        int Match(const std::u32string_view &input) override;

    private:
        std::u32string suffixes;
    };
}

#endif //GEDIT_GENERICNUMBERMATCHER_H

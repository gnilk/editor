//
// Created by gnilk on 02.06.2026.
//

#ifndef EDITOR_NUMBERMATCHERBASE_H
#define EDITOR_NUMBERMATCHERBASE_H

#include <memory>
#include <string>
#include <string_view>

namespace gedit {
    // Numbers can't be matched through a static identifier list - the lexical grammar must
    // be executed. A state may be given a NumberMatcher which is consulted in the tokenizer
    // before the regular identifier matching. The grammar varies per language, so each
    // language provides its own concrete matcher.
    class NumberMatcherBase {
    public:
        using Ref = std::shared_ptr<NumberMatcherBase>;
    public:
        NumberMatcherBase() = default;
        virtual ~NumberMatcherBase() = default;

        // Returns the number of characters from the start of 'input' that form a valid number.
        // Returns 0 if the input does not start with a number (the matcher owns the policy of
        // what may start a number, e.g. a leading '.').
        virtual int Match(const std::u32string_view &input) = 0;
    };
}

#endif //EDITOR_NUMBERMATCHERBASE_H

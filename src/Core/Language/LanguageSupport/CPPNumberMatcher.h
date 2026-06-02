//
// Created by gnilk on 02.06.2026.
//

#ifndef EDITOR_CPPNUMBERMATCHER_H
#define EDITOR_CPPNUMBERMATCHER_H

#include "Core/Language/NumberMatcherBase.h"

namespace gedit {
    // Matches C/C++ numeric literals:
    //   decimal:    123   1'000   3.14   .5   1.5e-3   2E10
    //   hex:        0xFF   0Xabc'123
    //   binary:     0b1010   0B0101
    //   suffixes:   123u 123UL 1.0f 1.0L 100z
    //   separators: digit-group separator ' (C++14)
    class CPPNumberMatcher : public NumberMatcherBase {
    public:
        using Ref = std::shared_ptr<CPPNumberMatcher>;
    public:
        CPPNumberMatcher() = default;
        virtual ~CPPNumberMatcher() = default;

        static NumberMatcherBase::Ref Create() {
            return std::make_shared<CPPNumberMatcher>();
        }

        int Match(const std::u32string_view &input) override;
    };
}

#endif //EDITOR_CPPNUMBERMATCHER_H

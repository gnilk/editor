//
// Created by gnilk on 11.06.26.
//
// The DATA half of auto-indentation: the trigger char sets + the electric-suppress classes, per language
// config name. Built from the indent.yml config (generic + inherit) or by hand in tests. Pure data +
// lookups - the decisions live in IndentEngine. An EMPTY table means "no language indent rules": the engine
// falls back to copying the previous line's indent and never dedents on type. See docs/indent-plan.md.
//

#ifndef EDITOR_INDENTTABLE_H
#define EDITOR_INDENTTABLE_H

#include <memory>
#include <set>

#include "Core/Language/LanguageTokenClass.h"

namespace gedit {

    class IndentTable {
    public:
        using Ref = std::shared_ptr<IndentTable>;
    public:
        std::set<char32_t> indentAfter;             // a line ENDING with one of these => next line +1 level
        std::set<char32_t> dedentOn;                // a line STARTING with one of these => that line -1 level
        std::set<kLanguageTokenClass> suppressIn;   // token classes (string/comment) where electric is OFF

        // Empty == no language rules; the engine just copies the previous indent and never dedents on type.
        bool IsEmpty() const {
            return indentAfter.empty() && dedentOn.empty();
        }
        bool IsIndentAfter(char32_t ch) const {
            return indentAfter.find(ch) != indentAfter.end();
        }
        bool IsDedentOn(char32_t ch) const {
            return dedentOn.find(ch) != dedentOn.end();
        }
        bool IsSuppressed(kLanguageTokenClass cls) const {
            return suppressIn.find(cls) != suppressIn.end();
        }
    };
}

#endif //EDITOR_INDENTTABLE_H

//
// Created by gnilk on 11.06.26.
//
// The DATA half of auto-pairing: the pair list + the guard rules, per language identifier. Built from
// the autopairs.yml config (generic + inherit) or by hand in tests. Pure data + lookups - the decisions
// live in AutoPairEngine. An EMPTY table means "no auto-pairing" (plaintext / unknown language), so a
// missing language section simply yields an empty table and the engine is a no-op.
//

#ifndef EDITOR_AUTOPAIRTABLE_H
#define EDITOR_AUTOPAIRTABLE_H

#include <memory>
#include <vector>
#include <set>

#include "Core/Language/LanguageTokenClass.h"

namespace gedit {

    // One open/close pair. For brackets open != close; for quotes open == close and isQuote is set, so the
    // engine knows the same key both opens and closes and must disambiguate (open-vs-skip).
    struct AutoPairRule {
        char32_t open = 0;
        char32_t close = 0;
        bool isQuote = false;
    };

    class AutoPairTable {
    public:
        using Ref = std::shared_ptr<AutoPairTable>;
    public:
        std::vector<AutoPairRule> pairs;
        std::set<char32_t> autoCloseBefore;             // auto-close fires when the next char is one of these
        bool autoCloseBeforeEol = true;                 // ...or at end-of-line
        std::set<kLanguageTokenClass> suppressIn;       // token classes (string/comment) where auto-close is OFF

        bool IsEmpty() const {
            return pairs.empty();
        }
        const AutoPairRule *FindByOpen(char32_t ch) const {
            for (auto &p : pairs) {
                if (p.open == ch) {
                    return &p;
                }
            }
            return nullptr;
        }
        const AutoPairRule *FindByClose(char32_t ch) const {
            for (auto &p : pairs) {
                if (p.close == ch) {
                    return &p;
                }
            }
            return nullptr;
        }
        bool IsSuppressed(kLanguageTokenClass cls) const {
            return suppressIn.find(cls) != suppressIn.end();
        }
        // nextOrZero == 0 means end-of-line.
        bool AutoCloseBefore(char32_t nextOrZero) const {
            if (nextOrZero == 0) {
                return autoCloseBeforeEol;
            }
            return autoCloseBefore.find(nextOrZero) != autoCloseBefore.end();
        }
    };
}

#endif //EDITOR_AUTOPAIRTABLE_H

//
// Created by gnilk on 11.06.26.
//
// The LOGIC half of auto-pairing: a single pure decision per keystroke. No UI, no tokenizer, no Line -
// everything it needs is in Context, so it is unit-tested directly (like HexView::ComputeNavTarget). The
// controller applies exactly one returned Action. See docs/autopair-plan.md.
//

#ifndef EDITOR_AUTOPAIRENGINE_H
#define EDITOR_AUTOPAIRENGINE_H

#include <string_view>

#include "Core/Language/AutoPairTable.h"
#include "Core/Language/LanguageTokenClass.h"

namespace gedit {

    class AutoPairEngine {
    public:
        enum class kPairAction {
            kNone,           // proceed with default editing
            kInsertPair,     // insert open+close, cursor between
            kSkipOver,       // type-through: don't insert, step cursor over the existing closer
            kDeletePair,     // backspace on an empty pair: delete the char before AND the closer after
            kWrapSelection,  // selection active: surround it with open..close
        };
        struct Action {
            kPairAction type = kPairAction::kNone;
            char32_t open = 0;
            char32_t close = 0;
        };
        // Everything the decision needs - kept deliberately primitive (a u32string_view + the SINGLE token
        // class AT the cursor, NOT a Line::Ref or the line's full token spans) so the engine stays pure and
        // unit-testable with no UI / Line / tokenizer dependency.
        //
        // The trade-off is limited semantic disambiguation. The motivating case is C++ angle brackets: '<'
        // is a template / #include bracket OR a less-than / shift operator, and telling them apart needs the
        // SURROUNDING token semantics (e.g. is the token to the left a known type?). With only the class at
        // the cursor we can't do that, so '<>' is intentionally NOT an auto-pair for now (see
        // Assets/Resources/autopairs.yml, cpp section). If we ever want it, ENRICH this Context (e.g. add a
        // "token class to the left" or a small peek callback) rather than handing the engine a whole Line -
        // keep the purity.
        struct Context {
            const AutoPairTable *table = nullptr;
            std::u32string_view lineText;        // current line content (no trailing newline)
            int cursorX = 0;                     // char index into lineText
            bool selectionActive = false;
            kLanguageTokenClass tokenClassAtCursor = kLanguageTokenClass::kRegular;
        };

        static Action OnInsertChar(const Context &ctx, char32_t typed);
        static Action OnBackspace(const Context &ctx);

    private:
        static char32_t CharLeft(const Context &ctx);
        static char32_t CharRight(const Context &ctx);   // 0 == end-of-line
        static bool IsIdentChar(char32_t ch);
    };
}

#endif //EDITOR_AUTOPAIRENGINE_H

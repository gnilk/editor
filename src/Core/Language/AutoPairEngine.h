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
        // Everything the decision needs - kept primitive so tests build it without a live editor.
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

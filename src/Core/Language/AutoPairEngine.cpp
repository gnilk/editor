//
// Created by gnilk on 11.06.26.
//
#include "AutoPairEngine.h"

using namespace gedit;

// On a typed character, decide whether to auto-close a pair, type-through an existing closer, wrap a
// selection, or do nothing (default insert). Priority within each branch is documented inline.
AutoPairEngine::Action AutoPairEngine::OnInsertChar(const Context &ctx, char32_t typed) {
    Action none;
    if ((ctx.table == nullptr) || ctx.table->IsEmpty()) {
        return none;
    }
    char32_t right = CharRight(ctx);
    const AutoPairRule *asOpen = ctx.table->FindByOpen(typed);

    // Quotes first: the same key opens AND closes, so disambiguate before the bracket branches.
    if ((asOpen != nullptr) && asOpen->isQuote) {
        // Wrap takes priority over everything when there is a selection.
        if (ctx.selectionActive) {
            return { kPairAction::kWrapSelection, asOpen->open, asOpen->close };
        }
        // Closing an already-open quote -> type-through.
        if (right == typed) {
            return { kPairAction::kSkipOver, asOpen->open, asOpen->close };
        }
        // No new pair inside a string/comment (plain insert) or right after an identifier (don't, char lit).
        if (ctx.table->IsSuppressed(ctx.tokenClassAtCursor) || IsIdentChar(CharLeft(ctx))) {
            return none;
        }
        return { kPairAction::kInsertPair, asOpen->open, asOpen->close };
    }

    // Opening bracket.
    if (asOpen != nullptr) {
        if (ctx.selectionActive) {
            return { kPairAction::kWrapSelection, asOpen->open, asOpen->close };
        }
        if (ctx.table->IsSuppressed(ctx.tokenClassAtCursor)) {
            return none;
        }
        // Only auto-close when the next char invites it (EOL, whitespace, a closer, a separator) - never
        // in the middle of a word.
        if (ctx.table->AutoCloseBefore(right)) {
            return { kPairAction::kInsertPair, asOpen->open, asOpen->close };
        }
        return none;
    }

    // Closing bracket: type-through if it matches the char to the right.
    const AutoPairRule *asClose = ctx.table->FindByClose(typed);
    if ((asClose != nullptr) && (right == typed)) {
        return { kPairAction::kSkipOver, asClose->open, asClose->close };
    }
    return none;
}

// On backspace, delete both halves of an adjacent empty pair (the cursor sits between open and close).
AutoPairEngine::Action AutoPairEngine::OnBackspace(const Context &ctx) {
    Action none;
    if ((ctx.table == nullptr) || ctx.table->IsEmpty()) {
        return none;
    }
    char32_t left = CharLeft(ctx);
    char32_t right = CharRight(ctx);
    const AutoPairRule *asOpen = ctx.table->FindByOpen(left);
    if ((asOpen != nullptr) && (asOpen->close == right)) {
        return { kPairAction::kDeletePair, asOpen->open, asOpen->close };
    }
    return none;
}

char32_t AutoPairEngine::CharLeft(const Context &ctx) {
    if ((ctx.cursorX <= 0) || ((size_t)ctx.cursorX > ctx.lineText.size())) {
        return 0;
    }
    return ctx.lineText[ctx.cursorX - 1];
}

char32_t AutoPairEngine::CharRight(const Context &ctx) {
    if ((ctx.cursorX < 0) || ((size_t)ctx.cursorX >= ctx.lineText.size())) {
        return 0;   // end-of-line
    }
    return ctx.lineText[ctx.cursorX];
}

bool AutoPairEngine::IsIdentChar(char32_t ch) {
    return (ch == U'_') ||
           ((ch >= U'0') && (ch <= U'9')) ||
           ((ch >= U'a') && (ch <= U'z')) ||
           ((ch >= U'A') && (ch <= U'Z'));
}

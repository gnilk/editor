//
// Parameters and locals of a function, up to the cursor (docs/code-index.md §11.11 slice C). The index has no locals, and
// most chains start at one: 'document->', 'line->'. This reads the code tokens the highlighter already left on the lines,
// from the function's own line to the cursor, when a go-to asks - nothing per edit.
//
// Heuristic, as the structure pass is: blocks are tracked, so what a closed block declared is gone; a statement that has
// the shape 'Type name' declares. 'auto' keeps its initializer, for the resolver to type as a chain.
//

#ifndef GOATEDIT_LOCALSCANNER_H
#define GOATEDIT_LOCALSCANNER_H

#include <cstdint>
#include <string>
#include <vector>

#include "Core/Line.h"
#include "Core/Language/LanguageTokenClass.h"

namespace gedit {
    struct LocalVariable {
        std::u32string name;
        std::u32string type;            // as written (TypeRef::Spelling), empty for 'auto' - then the initializer says
        std::u32string initializer;     // 'auto x = <initializer>;' as text, or the range of 'for (auto x : <range>)'
        bool isElement = false;         // range-for: an element of the initializer, not the initializer
        uint32_t line = 0;
        uint32_t column = 0;
    };

    class LocalScanner {
    public:
        struct Token {
            std::u32string text;
            uint32_t line = 0;
            uint32_t column = 0;
            kLanguageTokenClass classification = kLanguageTokenClass::kUnknown;
        };
    public:
        // The line's code tokens before 'idxEndColumn' - comments, strings and preprocessor lines left out
        static void AppendLineTokens(const Line &line, uint32_t idxLine, size_t idxEndColumn, std::vector<Token> &tokens);
        // From the start of a function (its name's line) to the cursor: what is visible at the end, in declaration order
        static std::vector<LocalVariable> Scan(const std::vector<Token> &tokens);
    };
}

#endif //GOATEDIT_LOCALSCANNER_H

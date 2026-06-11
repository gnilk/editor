//
// Created by gnilk on 11.06.26.
//
// A "clean" line is one whose lexer state-stack is at baseline depth (<= 1): NOT inside a multi-line
// construct (block comment / multi-line string). These two seeks find the nearest clean line backward /
// forward from an edit point - the shared kernel behind both the syntax highlighter (resume parsing from a
// clean state - LangLineTokenizer::ComputeParseRegion) and the indent engine (reindent from a trusted
// anchor / stop at a safe boundary). The per-line state depth is stamped by the tokenizer
// (Line::GetStateStackDepth). Caller-specific policy (edge margins, selection extension) stays in the
// callers; this is only the depth-walk both share.
//

#ifndef EDITOR_SYNTAXREGION_H
#define EDITOR_SYNTAXREGION_H

#include <cstddef>
#include <vector>

#include "Core/Line.h"

namespace gedit {

    class SyntaxRegion {
    public:
        // Nearest clean line at or before idx-1 (returns 0 if none, or if idx is 0 / out of range). idx is the
        // line being edited; the search starts one line above it.
        static size_t SeekCleanBackward(const std::vector<Line::Ref> &lines, size_t idx) {
            if ((idx == 0) || (idx > lines.size())) {
                return 0;
            }
            size_t idxStart = idx - 1;
            while ((idxStart != 0) && (lines[idxStart]->GetStateStackDepth() > 1)) {
                idxStart--;
            }
            return idxStart;
        }

        // Nearest clean line at or after idx+1 (returns lines.size() if none). The search starts one line
        // below idx.
        static size_t SeekCleanForward(const std::vector<Line::Ref> &lines, size_t idx) {
            size_t idxEnd = idx + 1;
            while ((idxEnd < lines.size()) && (lines[idxEnd]->GetStateStackDepth() > 1)) {
                idxEnd++;
            }
            return (idxEnd < lines.size()) ? idxEnd : lines.size();
        }
    };
}

#endif //EDITOR_SYNTAXREGION_H

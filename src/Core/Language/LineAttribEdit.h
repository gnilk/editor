//
// Created by gnilk on 11.11.26.
//
// Rewriting a parsed line's attribute spans - shared by the languages whose post-pass
// (LanguageBase::OnPostProcessParsedLine) reclassifies line-anchored syntax: markdown headings and list
// markers, YAML keys and typed scalars. Spans are ascending by idxOrigString; a span runs to the next one.
//

#ifndef EDITOR_LINEATTRIBEDIT_H
#define EDITOR_LINEATTRIBEDIT_H

#include <vector>

#include "Core/Line.h"
#include "Core/Language/LanguageTokenClass.h"

namespace gedit {

    class LineAttribEdit {
    public:
        // The class effective at char index 'pos' (kRegular before the first span).
        static kLanguageTokenClass ClassAt(const std::vector<Line::LineAttrib> &attribs, int pos);

        // Replace the whole line's attributes with one span of 'cls'.
        static void ReclassifyWholeLine(const Line::Ref &line, kLanguageTokenClass cls);

        // Override the half-open char range [start, end) with 'cls', preserving spans outside it. The span at
        // 'end' gets back whatever class was effective there, so later content keeps its highlighting.
        static void ReclassifyRange(const Line::Ref &line, int start, int end, kLanguageTokenClass cls);

        // A single span (with theme colours) for 'cls' starting at char index 'idx'.
        static Line::LineAttrib MakeAttrib(int idx, kLanguageTokenClass cls);
    };
}

#endif //EDITOR_LINEATTRIBEDIT_H

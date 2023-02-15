//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_TEXTBUFFER_H
#define EDITOR_TEXTBUFFER_H

#include <vector>

#include "Core/Line.h"

namespace gedit {
    class TextBuffer {
    public:
        TextBuffer() = default;
        virtual ~TextBuffer() = default;

        std::vector<Line *> &Lines() { return lines; }

        Line *LineAt(size_t idxLine) {
            return lines[idxLine];
        }

        size_t NumLines() {
            return lines.size();
        }

    private:
        std::vector<Line *> lines;

        // Do not put the edit controller here - might make sense, but it will cause problems later
        // Example: split-window editing with same file => won't work
        // EditController *editController = nullptr;
    };
}


#endif //EDITOR_TEXTBUFFER_H

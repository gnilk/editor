//
// Created by gnilk on 31.01.23.
//

#ifndef EDITOR_BUFFER_H
#define EDITOR_BUFFER_H

#include "Core/Language/LanguageBase.h"
#include "Core/Line.h"

class Buffer {
public:

    void SetLanguage(LanguageBase *newLanguage) {
        language = newLanguage;
        Reparse();
    }
    void Reparse();
    const LanguageBase &LangParser() { return *language; }
    std::vector<Line *> &Lines() { return lines; }

    Line *LineAt(size_t idxLine) {
        return lines[idxLine];
    }
    size_t NumLines() {
        return lines.size();
    }


private:
    LanguageBase *language;
    std::vector<Line *> lines;
};


#endif //EDITOR_BUFFER_H

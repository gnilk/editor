//
// Created by gnilk on 11.11.26.
//
// Lifted out of MarkdownLanguage.cpp so YAML can share it - see LineAttribEdit.h.
//

#include "LineAttribEdit.h"
#include "Core/Editor.h"

using namespace gedit;

kLanguageTokenClass LineAttribEdit::ClassAt(const std::vector<Line::LineAttrib> &attribs, int pos) {
    kLanguageTokenClass cls = kLanguageTokenClass::kRegular;
    for (auto &a : attribs) {
        if (a.idxOrigString <= pos) {
            cls = a.tokenClass;
        } else {
            break;
        }
    }
    return cls;
}

void LineAttribEdit::ReclassifyWholeLine(const Line::Ref &line, kLanguageTokenClass cls) {
    auto &attribs = line->Attributes();
    attribs.clear();
    attribs.push_back(MakeAttrib(0, cls));
}

void LineAttribEdit::ReclassifyRange(const Line::Ref &line, int start, int end, kLanguageTokenClass cls) {
    auto &attribs = line->Attributes();
    int lineLen = static_cast<int>(line->Buffer().size());

    kLanguageTokenClass tailClass = ClassAt(attribs, end);

    std::vector<Line::LineAttrib> rebuilt;
    for (auto &a : attribs) {
        if (a.idxOrigString < start) {
            rebuilt.push_back(a);
        }
    }
    rebuilt.push_back(MakeAttrib(start, cls));
    if (end < lineLen) {
        rebuilt.push_back(MakeAttrib(end, tailClass));
        for (auto &a : attribs) {
            if (a.idxOrigString > end) {
                rebuilt.push_back(a);
            }
        }
    }
    attribs = rebuilt;
}

Line::LineAttrib LineAttribEdit::MakeAttrib(int idx, kLanguageTokenClass cls) {
    Line::LineAttrib attrib;
    attrib.idxOrigString = idx;
    attrib.tokenClass = cls;
    auto [fg, bg] = Editor::Instance().ColorFromLanguageToken(cls);
    attrib.foregroundColor = fg;
    attrib.backgroundColor = bg;
    return attrib;
}

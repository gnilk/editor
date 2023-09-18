//
// Created by gnilk on 31.03.23.
//

#include "DrawContext.h"
#include "logger.h"
#include "LineRender.h"
#include "Editor.h"
#include "Core/Editor.h"
using namespace gedit;

// This assumes X = 0
void LineRender::DrawLines(const std::vector<Line::Ref> &lines, int idxTopLine, int idxBottomLine, const Selection &selection) {
    auto &rect = dc.GetRect();
    auto &theme = Editor::Instance().GetTheme();
    auto contentColors = theme->GetContentColors();


    for (int i = idxTopLine; i < idxBottomLine; i++) {
        if (i >= (int)lines.size()) {
            break;
        }
        auto line = lines[i];

        // I've seen a crash ONCE where there was interference between this and the language parser
        // But I couldn't determine the crash, std::mutex (down the drain) would throw an exception..
//        if (line->IsLocked()) {
//            continue; // This is fishy - we should probably log this
//        }

        line->Lock();
        auto nCharToPrint = line->Length() > static_cast<size_t>(rect.Width()) ? static_cast<size_t>(rect.Width()) : line->Length();
        dc.ClearLine(i - idxTopLine);
        DrawLineWithAttributesAt(0, i - idxTopLine, nCharToPrint, *line, selection);


        dc.SetFGColor(contentColors["selection"]);
        dc.DrawLineOverlays(i - idxTopLine);
        line->Release();
    }
}
void LineRender::DrawLine(int x, int y, const Line::Ref line) {
    auto &rect = dc.GetRect();
    auto &theme = Editor::Instance().GetTheme();
    auto contentColors = theme->GetContentColors();

    line->Lock();
    auto nCharToPrint = line->Length() > static_cast<size_t>(rect.Width()) ? static_cast<size_t>(rect.Width()) : line->Length();

    DrawLineWithAttributesAt(x,y, line);

    line->Release();
}

// This is the more advanced drawing routine...
void LineRender::DrawLineWithAttributesAt(int x, int y, const Line::Ref line) {

    // No attributes?  Just dump the string...
    auto &attribs = line->Attributes();
    if (attribs.size() == 0) {
        dc.DrawStringAt(x, y, line->Buffer().data());
        return;
    }

    // We split the line in attribute chunks and draw partial lines...
    auto itAttrib = attribs.begin();
    int xp = x;

    //
    // FIXME: Consider breaking this while loop out and put it Line instead with a Lambda expression
    //       line->IterateAttributes([](LineAttribIterator iterator, std::u32string &str) { });
    //
    while (itAttrib != attribs.end()) {
        auto next = itAttrib + 1;
        size_t len = std::string::npos;
        // Not at the end - replace with length of this attribute
        if (next != attribs.end()) {
            // Some kind of assert!
            if (itAttrib->idxOrigString > next->idxOrigString) {
                auto logger = gnilk::Logger::GetLogger("SDLDrawContext");
                logger->Error("DrawLineWithAttributesAt, attribute index is wrong for line: '%s'", UnicodeHelper::utf32to8(line->Buffer()).c_str());
                return;
            }
            len = next->idxOrigString - itAttrib->idxOrigString;
        }
        // we need reparse!
        if (static_cast<size_t>(itAttrib->idxOrigString) > line->Length()) {
            return;
        }
        // Grab the substring for this attribute range
        auto strOut = std::u32string(line->Buffer(), itAttrib->idxOrigString, len);

        // Draw string with the correct color...
        auto [fgColor, bgColor] = Editor::Instance().ColorFromLanguageToken(itAttrib->tokenClass);
        dc.SetColor(fgColor, bgColor);
        dc.DrawStringWithAttributesAt(xp,y, itAttrib->textAttributes, strOut);

        xp += len;
        itAttrib = next;
    }

}


// This is the more advanced drawing routine...
void LineRender::DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l, const Selection &selection) {

    // No attributes?  Just dump the string...
    auto &attribs = l.Attributes();
    if (attribs.size() == 0) {
        dc.DrawStringAt(x, y, l.Buffer().data());
        return;
    }

    // We split the line in attribute chunks and draw partial lines...
    auto itAttrib = attribs.begin();
    int xp = x;

    while (itAttrib != attribs.end()) {
        auto next = itAttrib + 1;
        size_t len = std::string::npos;
        // Not at the end - replace with length of this attribute
        if (next != attribs.end()) {
            // Some kind of assert!
            if (itAttrib->idxOrigString > next->idxOrigString) {
                auto logger = gnilk::Logger::GetLogger("SDLDrawContext");
                logger->Error("DrawLineWithAttributesAt, attribute index is wrong for line: '%s'", l.Buffer().data());
                return;
            }
            len = next->idxOrigString - itAttrib->idxOrigString;
        }
        // we need reparse!
        if (static_cast<size_t>(itAttrib->idxOrigString) > l.Length()) {
            return;
        }
        // Grab the substring for this attribute range
        auto strOut = std::u32string(l.Buffer().data(), itAttrib->idxOrigString, len);

        // Draw string with the correct color...
        auto [fgColor, bgColor] = Editor::Instance().ColorFromLanguageToken(itAttrib->tokenClass);
        dc.SetColor(fgColor, bgColor);
        dc.DrawStringWithAttributesAt(xp,y, itAttrib->textAttributes, strOut);

        xp += len;
        itAttrib = next;
    }

}


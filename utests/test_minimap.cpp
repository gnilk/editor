//
// Minimap (docs/usability-round.md §7 (b)) - lines and their colour spans into runs of pixels and an image, on strings
//
#include <testinterface.h>

#include <string>
#include <vector>

#include "Core/Line.h"
#include "Core/Minimap.h"
#include "Core/OverviewMap.h"
#include "Core/UI/Graphics/PixelImage.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_minimap(ITesting *t);
    DLL_EXPORT int test_minimap_runs(ITesting *t);
    DLL_EXPORT int test_minimap_runsspans(ITesting *t);
    DLL_EXPORT int test_minimap_runstabs(ITesting *t);
    DLL_EXPORT int test_minimap_runswidth(ITesting *t);
    DLL_EXPORT int test_minimap_image(ITesting *t);
    DLL_EXPORT int test_minimap_imageshared(ITesting *t);
    DLL_EXPORT int test_minimap_imagewide(ITesting *t);
    DLL_EXPORT int test_minimap_imageempty(ITesting *t);
    DLL_EXPORT int test_minimap_compose(ITesting *t);
    DLL_EXPORT int test_minimap_textstrength(ITesting *t);
}

namespace {
    const ColorRGBA kRed = ColorRGBA::FromRGB(255, 0, 0);
    const ColorRGBA kBlue = ColorRGBA::FromRGB(0, 0, 255);
    const ColorRGBA kWhite = ColorRGBA::FromRGB(255, 255, 255);
    const ColorRGBA kBlack = ColorRGBA::FromRGB(0, 0, 0);

    constexpr uint32_t kPixelRed = 0xff0000ff;
    constexpr uint32_t kPixelBlue = 0x0000ffff;
    constexpr uint32_t kPixelWhite = 0xffffffff;
    constexpr uint32_t kPixelBlack = 0x000000ff;

    // A line with spans: each (start, colour)
    Line::Ref LineWith(const std::u32string &text, const std::vector<std::pair<int, ColorRGBA>> &spans) {
        auto line = Line::Create(text);
        for (auto &[start, colour] : spans) {
            Line::LineAttrib attrib;
            attrib.idxOrigString = start;
            attrib.foregroundColor = colour;
            line->Attributes().push_back(attrib);
        }
        return line;
    }

    // The runs as a string: "x+len" each, ' ' between
    std::string RunsText(const Minimap::Runs &runs) {
        std::string out;
        for (auto &run : runs) {
            if (!out.empty()) {
                out += ' ';
            }
            out += std::to_string(run.x) + "+" + std::to_string(run.length);
        }
        return out;
    }

    Minimap::Style StyleOf(int pixelsPerChar = 1) {
        Minimap::Style style;
        style.pixelsPerChar = pixelsPerChar;
        style.tabSize = 4;
        style.background = kBlack;
        style.foreground = kWhite;
        return style;
    }
}

DLL_EXPORT int test_minimap(ITesting *t) {
    return kTR_Pass;
}

// Whitespace is left out, every other character a column; no spans - the default colour
DLL_EXPORT int test_minimap_runs(ITesting *t) {
    auto line = Line::Create(U"  int a = 1;");
    auto runs = Minimap::RunsOf(*line, 4, 100, kWhite);
    TR_ASSERT(t, RunsText(runs) == "2+3 6+1 8+1 10+2");
    TR_ASSERT(t, runs[0].colour == kWhite);
    TR_ASSERT(t, Minimap::RunsOf(*Line::Create(U""), 4, 100, kWhite).empty());
    TR_ASSERT(t, Minimap::RunsOf(*Line::Create(U"    "), 4, 100, kWhite).empty());
    return kTR_Pass;
}

// A run ends where a span changes colour, and a span's colour carries over the whitespace into the next word
DLL_EXPORT int test_minimap_runsspans(ITesting *t) {
    auto line = LineWith(U"int abc;", {{0, kRed}, {3, kBlue}});
    auto runs = Minimap::RunsOf(*line, 4, 100, kWhite);
    TR_ASSERT(t, RunsText(runs) == "0+3 4+4");
    TR_ASSERT(t, runs[0].colour == kRed);
    TR_ASSERT(t, runs[1].colour == kBlue);
    // Two spans of one colour next to each other are one run
    auto same = LineWith(U"abcd", {{0, kRed}, {2, kRed}});
    TR_ASSERT(t, RunsText(Minimap::RunsOf(*same, 4, 100, kWhite)) == "0+4");
    return kTR_Pass;
}

// A tab goes to the next tab stop, as in the editor
DLL_EXPORT int test_minimap_runstabs(ITesting *t) {
    auto line = Line::Create(U"\tx\ty");
    TR_ASSERT(t, RunsText(Minimap::RunsOf(*line, 4, 100, kWhite)) == "4+1 8+1");
    auto mid = Line::Create(U"ab\tc");
    TR_ASSERT(t, RunsText(Minimap::RunsOf(*mid, 4, 100, kWhite)) == "0+2 4+1");
    return kTR_Pass;
}

// A long line is cut at the minimap's width
DLL_EXPORT int test_minimap_runswidth(ITesting *t) {
    auto line = Line::Create(std::u32string(50, U'x'));
    auto runs = Minimap::RunsOf(*line, 4, 10, kWhite);
    TR_ASSERT(t, RunsText(runs) == "0+10");
    auto tabbed = Line::Create(U"\t\t\tx");
    TR_ASSERT(t, Minimap::RunsOf(*tabbed, 4, 10, kWhite).empty());
    return kTR_Pass;
}

// A row or more a line: each character a pixel in its colour on every row of its line, the rest the background
DLL_EXPORT int test_minimap_image(ITesting *t) {
    std::vector<Line::Ref> lines = {LineWith(U"ab c", {{0, kRed}, {3, kBlue}}), Line::Create(U""), Line::Create(U" x")};
    OverviewMap map(lines.size(), 100);         // two rows a line
    PixelImage image;
    Minimap::Render(image, 8, map, lines, StyleOf());
    TR_ASSERT(t, (image.Width() == 8) && (image.Height() == 100));
    for (int row : {0, 1}) {
        TR_ASSERT(t, image.PixelAt(0, row) == kPixelRed);
        TR_ASSERT(t, image.PixelAt(1, row) == kPixelRed);
        TR_ASSERT(t, image.PixelAt(2, row) == kPixelBlack);
        TR_ASSERT(t, image.PixelAt(3, row) == kPixelBlue);
    }
    TR_ASSERT(t, image.PixelAt(0, 2) == kPixelBlack);       // the empty line
    TR_ASSERT(t, image.PixelAt(1, 4) == kPixelWhite);       // ' x', no spans
    TR_ASSERT(t, image.PixelAt(0, 4) == kPixelBlack);
    TR_ASSERT(t, image.PixelAt(1, 6) == kPixelBlack);       // below the file
    return kTR_Pass;
}

// Several lines a row: a column all of them fill is at full strength, one only some fill is blended toward the
// background
DLL_EXPORT int test_minimap_imageshared(ITesting *t) {
    std::vector<Line::Ref> lines = {Line::Create(U"xx"), Line::Create(U"x"), Line::Create(U"xx"), Line::Create(U"x")};
    OverviewMap map(lines.size(), 2);           // two lines a row
    PixelImage image;
    Minimap::Render(image, 4, map, lines, StyleOf());
    TR_ASSERT(t, image.Height() == 2);
    for (int row : {0, 1}) {
        TR_ASSERT(t, image.PixelAt(0, row) == kPixelWhite);
        auto half = image.PixelAt(1, row);
        auto red = (half >> 24) & 0xff;
        TR_ASSERT(t, (red > 100) && (red < 155));
        TR_ASSERT(t, image.PixelAt(2, row) == kPixelBlack);
    }
    return kTR_Pass;
}

// A character two pixels wide (a 2x display)
DLL_EXPORT int test_minimap_imagewide(ITesting *t) {
    std::vector<Line::Ref> lines = {Line::Create(U"a b")};
    OverviewMap map(lines.size(), 10);
    PixelImage image;
    Minimap::Render(image, 10, map, lines, StyleOf(2));
    TR_ASSERT(t, image.PixelAt(0, 0) == kPixelWhite);
    TR_ASSERT(t, image.PixelAt(1, 0) == kPixelWhite);
    TR_ASSERT(t, image.PixelAt(2, 0) == kPixelBlack);
    TR_ASSERT(t, image.PixelAt(3, 0) == kPixelBlack);
    TR_ASSERT(t, image.PixelAt(4, 0) == kPixelWhite);
    TR_ASSERT(t, image.PixelAt(5, 0) == kPixelWhite);
    return kTR_Pass;
}

// No lines, or no room: an image of the background (or none), and a new revision each time it is built
DLL_EXPORT int test_minimap_imageempty(ITesting *t) {
    std::vector<Line::Ref> none;
    PixelImage image;
    Minimap::Render(image, 8, OverviewMap(0, 10), none, StyleOf());
    TR_ASSERT(t, image.Height() == 10);
    TR_ASSERT(t, image.PixelAt(3, 3) == kPixelBlack);
    auto revision = image.Revision();
    std::vector<Line::Ref> lines = {Line::Create(U"x")};
    Minimap::Render(image, 0, OverviewMap(1, 10), lines, StyleOf());
    TR_ASSERT(t, image.IsEmpty());
    TR_ASSERT(t, image.Revision() != revision);
    return kTR_Pass;
}

// The frame is composed from the text image (CopyFrom, cut at the edges) and translucent fills (BlendRect)
DLL_EXPORT int test_minimap_compose(ITesting *t) {
    PixelImage text;
    text.Reset(4, 2, kWhite);
    PixelImage frame;
    frame.Reset(6, 3, kBlack);
    frame.CopyFrom(text, 3, 2);
    TR_ASSERT(t, frame.PixelAt(2, 2) == kPixelBlack);
    TR_ASSERT(t, frame.PixelAt(3, 2) == kPixelWhite);
    TR_ASSERT(t, frame.PixelAt(5, 2) == kPixelWhite);
    TR_ASSERT(t, frame.PixelAt(3, 1) == kPixelBlack);

    frame.BlendRect(0, 0, 2, 1, kRed);                      // opaque: replaces
    TR_ASSERT(t, frame.PixelAt(0, 0) == kPixelRed);
    TR_ASSERT(t, frame.PixelAt(2, 0) == kPixelBlack);
    frame.BlendRect(-5, 1, 100, 1, ColorRGBA::FromRGBA(255, 255, 255, 128));   // half: between, cut at the edges
    auto grey = (frame.PixelAt(0, 1) >> 24) & 0xff;
    TR_ASSERT(t, (grey > 110) && (grey < 145));
    TR_ASSERT(t, (frame.PixelAt(0, 1) & 0xff) == 0xff);
    return kTR_Pass;
}

// Damped text (usability-round §7 follow-up): a full line comes out between the background and its colour, by the
// strength; 1 is the colour itself, 0 the background
DLL_EXPORT int test_minimap_textstrength(ITesting *t) {
    std::vector<Line::Ref> lines = {LineWith(U"ab", {{0, kWhite}})};
    OverviewMap map(lines.size(), 2);
    PixelImage image;
    auto style = StyleOf();

    Minimap::Render(image, 4, map, lines, style);
    TR_ASSERT(t, image.PixelAt(0, 0) == kPixelWhite);

    style.textStrength = 0.4f;
    Minimap::Render(image, 4, map, lines, style);
    auto grey = (image.PixelAt(0, 0) >> 24) & 0xff;
    TR_ASSERT(t, (grey >= 100) && (grey <= 104));           // 0.4 * 255
    TR_ASSERT(t, image.PixelAt(2, 0) == kPixelBlack);       // no character: the background, as before

    style.textStrength = 0.0f;
    Minimap::Render(image, 4, map, lines, style);
    TR_ASSERT(t, image.PixelAt(0, 0) == kPixelBlack);
    return kTR_Pass;
}

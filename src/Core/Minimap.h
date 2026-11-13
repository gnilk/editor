//
// Minimap - the file's text drawn small (docs/usability-round.md §7 (b)): a pixel per character, coloured as the
// highlighter coloured it, whitespace left out. Built from what the lines carry - their attribute spans - so no lexing
// and no copy of the text. Plain: lines in, a PixelImage out; the backend only draws the image.
//
// The rows are the OverviewMap's: the whole file fits the height. A line a row or more is drawn on each of its rows; a
// row holding several lines blends them - a column's colour is the average of the characters there, its strength how
// many of the row's lines have one, so a dense block reads darker than a sparse one.
//
#ifndef GEDIT_MINIMAP_H
#define GEDIT_MINIMAP_H

#include <vector>

#include "Core/ColorRGBA.h"
#include "Core/Line.h"
#include "Core/OverviewMap.h"
#include "Core/UI/Graphics/PixelImage.h"

namespace gedit {

    class Minimap {
    public:
        // 'length' columns from column 'x', one colour
        struct Run {
            int x = 0;
            int length = 0;
            ColorRGBA colour;
        };
        using Runs = std::vector<Run>;

        struct Style {
            int pixelsPerChar = 1;              // the width of a character, in the image's pixels
            int tabSize = 4;
            ColorRGBA background;
            ColorRGBA foreground;               // a line without spans (no language)
            // How strongly the text is laid over the background, 0..1 - below 1 the text reads as texture and the
            // marks and the box drawn over it stand out (usability-round §7 follow-up; VS Code is about 0.4)
            float textStrength = 1.0f;
        };

    public:
        // A line's characters as runs of columns: whitespace left out, tabs expanded as the editor does, the colour of
        // the span each character is in. At most 'maxColumns' columns. The caller holds the line's lock.
        static Runs RunsOf(const Line &line, int tabSize, int maxColumns, const ColorRGBA &defaultColour);

        // The lines into 'image', the rows mapped by 'map' (map.NumRows() high): the image is reset to the background
        // first, so it is the whole picture. Each line is locked while it is read.
        static void Render(PixelImage &image, int width, const OverviewMap &map, const std::vector<Line::Ref> &lines,
                           const Style &style);

    private:
        static void RenderRow(PixelImage &image, int row, size_t idxFirstLine, size_t idxEndLine,
                              const std::vector<Line::Ref> &lines, const Style &style);
    };

}

#endif //GEDIT_MINIMAP_H

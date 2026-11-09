//
// Minimap - see Minimap.h
//
#include "Minimap.h"

#include <algorithm>

using namespace gedit;

void Minimap::Render(PixelImage &image, int width, const OverviewMap &map, const std::vector<Line::Ref> &lines,
                     const Style &style) {
    image.Reset(width, map.NumRows(), style.background);
    auto numLines = std::min(map.NumLines(), lines.size());
    if ((numLines == 0) || image.IsEmpty()) {
        return;
    }
    for (int row = 0; row < map.UsedRows(); row++) {
        // The lines drawn on this row: from the first one in it to the first one starting below it
        auto idxFirstLine = map.LineOfRow(row);
        auto idxEndLine = idxFirstLine + 1;
        while ((idxEndLine < numLines) && (map.RowOfLine(idxEndLine) <= row)) {
            idxEndLine++;
        }
        RenderRow(image, row, idxFirstLine, std::min(idxEndLine, numLines), lines, style);
    }
}

// A column's colour is the average of the row's characters there, laid over the background as strongly as the share of
// the row's lines that have one
void Minimap::RenderRow(PixelImage &image, int row, size_t idxFirstLine, size_t idxEndLine,
                        const std::vector<Line::Ref> &lines, const Style &style) {
    auto pixelsPerChar = std::max(style.pixelsPerChar, 1);
    auto numColumns = image.Width() / pixelsPerChar;
    if ((numColumns <= 0) || (idxFirstLine >= idxEndLine)) {
        return;
    }
    std::vector<float> sumR((size_t)numColumns, 0.0f);
    std::vector<float> sumG((size_t)numColumns, 0.0f);
    std::vector<float> sumB((size_t)numColumns, 0.0f);
    std::vector<int> count((size_t)numColumns, 0);

    for (auto idxLine = idxFirstLine; idxLine < idxEndLine; idxLine++) {
        auto &line = lines[idxLine];
        line->Lock();
        auto runs = RunsOf(*line, style.tabSize, numColumns, style.foreground);
        line->Release();
        for (auto &run : runs) {
            for (int x = run.x; x < (run.x + run.length); x++) {
                sumR[(size_t)x] += (float)run.colour.RedAsInt();
                sumG[(size_t)x] += (float)run.colour.GreenAsInt();
                sumB[(size_t)x] += (float)run.colour.BlueAsInt();
                count[(size_t)x]++;
            }
        }
    }

    auto numLinesInRow = (float)(idxEndLine - idxFirstLine);
    auto bgR = (float)style.background.RedAsInt();
    auto bgG = (float)style.background.GreenAsInt();
    auto bgB = (float)style.background.BlueAsInt();
    for (int column = 0; column < numColumns; column++) {
        auto n = count[(size_t)column];
        if (n == 0) {
            continue;
        }
        auto strength = (float)n / numLinesInRow;
        auto r = (uint8_t)(bgR + ((sumR[(size_t)column] / (float)n) - bgR) * strength);
        auto g = (uint8_t)(bgG + ((sumG[(size_t)column] / (float)n) - bgG) * strength);
        auto b = (uint8_t)(bgB + ((sumB[(size_t)column] / (float)n) - bgB) * strength);
        for (int px = 0; px < pixelsPerChar; px++) {
            image.SetPixel(column * pixelsPerChar + px, row, r, g, b, 0xff);
        }
    }
}

Minimap::Runs Minimap::RunsOf(const Line &line, int tabSize, int maxColumns, const ColorRGBA &defaultColour) {
    Runs runs;
    auto &text = line.Buffer();
    auto &attribs = line.Attributes();
    tabSize = std::max(tabSize, 1);
    size_t idxSpan = 0;
    int column = 0;
    for (size_t idxChar = 0; (idxChar < text.size()) && (column < maxColumns); idxChar++) {
        auto ch = text[idxChar];
        if (ch == U'\t') {
            column += tabSize - (column % tabSize);
            continue;
        }
        if ((ch == U' ') || (ch == U'\r') || (ch == U'\n') || (ch == U' ')) {
            column++;
            continue;
        }
        // The span the character is in: the last one starting at or before it
        while (((idxSpan + 1) < attribs.size()) && ((size_t)attribs[idxSpan + 1].idxOrigString <= idxChar)) {
            idxSpan++;
        }
        auto &colour = attribs.empty() ? defaultColour : attribs[idxSpan].foregroundColor;
        if (!runs.empty() && ((runs.back().x + runs.back().length) == column) && (runs.back().colour == colour)) {
            runs.back().length++;
        } else {
            runs.push_back({column, 1, colour});
        }
        column++;
    }
    return runs;
}

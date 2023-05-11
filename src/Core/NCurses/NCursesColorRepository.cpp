//
// Created by gnilk on 11.05.23.
//

#include <ncurses.h>
#include "logger.h"

#include "NCursesColorRepository.h"

using namespace gedit;
int NCursesColorRepository::GetColorPairIndex(const ColorRGBA &fgColor, const ColorRGBA &bgColor) {
    ColorPair pair = {fgColor, bgColor};
    if (colorPairs.find(pair) != colorPairs.end()) {
        return colorPairs[pair];
    }
    auto logger = gnilk::Logger::GetLogger("NCursesColorRepo");

    int err = 0;
    int idxFg = colorIndex++;
    int idxBg = colorIndex++;
    err = init_color(idxFg, fgColor.RedAsInt(1000), fgColor.GreenAsInt(1000), fgColor.BlueAsInt(1000));
    if (err == ERR) {
        logger->Error("failed to initialize color with index %d", idxFg);
    }
    err = init_color(idxBg, bgColor.RedAsInt(1000), bgColor.GreenAsInt(1000), bgColor.BlueAsInt(1000));
    if (err == ERR) {
        logger->Error("failed to initialize color with index %d", idxBg);
    }

    int newColorIndex = colorPairIndex;
    err = init_pair(newColorIndex, idxFg, idxBg);
    if (err == ERR) {
        logger->Debug("failed to initialize color pair for colorIndex: %d", newColorIndex);
    }
    colorPairs[pair] = newColorIndex;

    colorPairIndex++;
    return newColorIndex;
}

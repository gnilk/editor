//
// Created by gnilk on 11.05.23.
//

#include <ncurses.h>
#include "logger.h"

#include "NCursesColorRepository.h"

using namespace gedit;
NCursesColorRepository &NCursesColorRepository::Instance() {
    static NCursesColorRepository glbColorRepo;
    return glbColorRepo;
}

int NCursesColorRepository::GetColorPairIndex(const ColorRGBA &fgColor, const ColorRGBA &bgColor) {
    ColorPair pair = {fgColor, bgColor};
    if (colorPairs.find(pair) != colorPairs.end()) {
        return colorPairs[pair];
    }
    auto logger = gnilk::Logger::GetLogger("NCursesColorRepo");


    int err = 0;
    int idxFg = colorIndex;
    int idxBg = colorIndex+1;
    err = init_color(idxFg, fgColor.RedAsInt(999), fgColor.GreenAsInt(999), fgColor.BlueAsInt(999));
    if (err == ERR) {
        logger->Error("failed to initialize color with index %d", idxFg);
    }
    err = init_color(idxBg, bgColor.RedAsInt(999), bgColor.GreenAsInt(999), bgColor.BlueAsInt(999));
    if (err == ERR) {
        logger->Error("failed to initialize color with index %d", idxBg);
    }

    logger->Debug("NewColorPair, idxPair: %d, idxFg; %d, idxBg: %d (ci: %d, cpi: %d)", colorPairIndex, idxFg, idxBg, colorIndex, colorPairIndex);

    int newColorIndex = colorPairIndex;
    err = init_pair(newColorIndex, idxFg, idxBg);
    if (err == ERR) {
        logger->Debug("failed to initialize color pair for colorIndex: %d", newColorIndex);
    }
    colorPairs[pair] = newColorIndex;

    colorIndex+=2;
    colorPairIndex++;
    return newColorIndex;
}

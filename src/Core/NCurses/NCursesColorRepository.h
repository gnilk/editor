//
// Created by gnilk on 11.05.23.
//

#ifndef EDITOR_NCURSESCOLORREPOSITORY_H
#define EDITOR_NCURSESCOLORREPOSITORY_H

#include <unordered_map>
#include <functional>
#include "Core/ColorRGBA.h"

namespace gedit {
    class NCursesColorRepository {
    public:
        struct ColorPair {
            ColorRGBA fg;
            ColorRGBA bg;
        };
        struct ColorPairHash {
            std::size_t operator()(const ColorPair& col) const {
                return (col.fg.Hash()) ^ (col.bg.Hash() << 1);
            }
        };

        struct ColorPairEqual {
            bool operator()(const ColorPair& lhs, const ColorPair& rhs) const {
                return lhs.fg == rhs.fg && lhs.bg == rhs.bg;
            }
        };
    public:
        NCursesColorRepository() = default;
        virtual ~NCursesColorRepository() = default;

        static NCursesColorRepository &Instance();

        int GetColorPairIndex(const ColorRGBA &fgColor, const ColorRGBA &bgColor);
    private:
        int colorIndex = 1;
        int colorPairIndex = 1;
        std::unordered_map<ColorPair, int, ColorPairHash, ColorPairEqual> colorPairs;
    };
}


#endif //EDITOR_NCURSESCOLORREPOSITORY_H

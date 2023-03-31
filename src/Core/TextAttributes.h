//
// Created by gnilk on 30.03.23.
//

#ifndef EDITOR_TEXTATTRIBUTES_H
#define EDITOR_TEXTATTRIBUTES_H

#include <vector>

namespace gedit {
    enum class kTextAttributes : uint16_t {
        kNormal = 1,
        kInverted = 2,
        kBold = 4,
        kItalic = 8,
        kUnderline = 16,
    };

    inline kTextAttributes operator|(kTextAttributes lhs, kTextAttributes rhs) {
        return static_cast<kTextAttributes>(
                static_cast<std::underlying_type_t<kTextAttributes>>(lhs) |
                static_cast<std::underlying_type_t<kTextAttributes>>(rhs)
        );
    }

    // Should this return bool or kTextAttributes
    inline bool operator&(kTextAttributes lhs, kTextAttributes rhs) {
        return static_cast<bool>(
                static_cast<std::underlying_type_t<kTextAttributes>>(lhs) &
                static_cast<std::underlying_type_t<kTextAttributes>>(rhs)
        );
    }


    struct TextAttribute {
        kTextAttributes attribute;
        int idxColor;
    };

}

#endif //EDITOR_TEXTATTRIBUTES_H

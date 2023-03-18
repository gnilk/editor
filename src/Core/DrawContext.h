//
// Created by gnilk on 22.02.23.
//

#ifndef NCWIN_DRAWCONTEXT_H
#define NCWIN_DRAWCONTEXT_H

#include <vector>

#include "Core/NativeWindow.h"
#include "Core/Line.h"
#include "Core/Rect.h"

namespace gedit {
    enum class kTextAttributes : uint8_t {
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

    class DrawContext {
    public:
        DrawContext() = default;
        explicit DrawContext(NativeWindow window, Rect clientRect) : win(window), rect(clientRect) {
        }
        virtual ~DrawContext() = default;
        virtual void DrawStringAt(int x, int y, const char *str) {}
        virtual void DrawStringWithAttributesAt(int x, int y, kTextAttributes attrib, const char *str) {}
        virtual void DrawLine(Line *line, int idxLine) {}
        virtual void DrawLines(const std::vector<Line *> &lines, int idxTopLine, int idxBottomLine) {}
        virtual void DrawLineWithAttributesAt(int x, int y, int nCharToPrint, Line &l) {}

        virtual void ClearLine(int y) {}
        virtual void FillLine(int y, kTextAttributes attrib, char c) {}

        virtual void Clear() {}
        virtual void Scroll(int nRows) {}
        virtual const Rect &GetRect() {
            return rect;
        }
    protected:
        Rect rect = {};
        NativeWindow win = nullptr;
    };
}
#endif //NCWIN_DRAWCONTEXT_H

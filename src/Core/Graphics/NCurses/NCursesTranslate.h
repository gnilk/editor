//
// Created by gnilk on 12.05.23.
//

#ifndef GEDIT_NCURSESTRANSLATE_H
#define GEDIT_NCURSESTRANSLATE_H

#include "NCursesScreen.h"

namespace gedit {
    class NCursesTranslate {
        friend NCursesScreen;
    public:
        static void PixelToRowCol(float &x, float &y) {
            x = x * fac_x_to_rc;
            y = y * fac_y_to_rc;
        }
        static Point PixelToRowCol(const Point &pnt) {
            Point rc;
            rc.x = pnt.x * fac_x_to_rc;
            rc.y = pnt.y * fac_y_to_rc;
            return rc;
        }
        static Rect PixelToRowCol(const Rect &src) {
            Rect dst;
            dst = {(int)(src.Width() * fac_x_to_rc),  (int)(src.Height() * fac_y_to_rc)};
            return dst;
        }

        static void RowColToPixel(float &x, float &y) {
            x = x / fac_x_to_rc;
            y = y / fac_y_to_rc;
        }

        static Point RowColToPixel(const Point &pnt) {
            Point pix;
            pix.x = pnt.x / fac_x_to_rc;
            pix.y = pnt.y / fac_y_to_rc;
            return pix;
        }

        static Rect RowColToPixel(const Rect &src) {
            Rect dst;
            auto pixTopLeft = RowColToPixel(src.TopLeft());
            dst = {(int)(src.Width() / fac_x_to_rc),  (int)(src.Height() / fac_y_to_rc)};
            dst.Move(pixTopLeft.x, pixTopLeft.y);
            return dst;
        }


        static float XPosToRow(float x) {
            return x * fac_x_to_rc;
        }
        static float YPosToCol(float y) {
            return y * fac_y_to_rc;
        }
        static float ColToXPos(int col) {
            return col / fac_x_to_rc;
        }
        static float RowToYPos(int row) {
            return row / fac_y_to_rc;
        }
    protected:
        static float fac_x_to_rc;
        static float fac_y_to_rc;

    };
}

#endif //EDITOR_NCURSESTRANSLATE_H

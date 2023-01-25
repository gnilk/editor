//
// Created by gnilk on 25.01.23.
//

#include "Core/ColorRGBA.h"

// helper....
static float HueToRGB(float v1, float v2, float vH) {
    if (vH < 0)
        vH += 1;

    if (vH > 1)
        vH -= 1;

    if ((6 * vH) < 1)
        return (v1 + (v2 - v1) * 6 * vH);

    if ((2 * vH) < 1)
        return v2;

    if ((3 * vH) < 2)
        return (v1 + (v2 - v1) * ((2.0f / 3) - vH) * 6);

    return v1;
}

//
// h 0..360
// s 0..1 (percentage)
// l 0..1 (percentage)
//
ColorRGBA ColorRGBA::FromHSL(float h, float s, float l) {
    ColorRGBA col;

    // clip s/l
    if (s < 0.0f) s = 0.0f;
    else if ( s > 1.0f) s = 1.0f;
    if (l < 0.0f) s = 0.0f;
    else if (l > 1.0f) l = 1.0f;

    if (s == 0)
    {
        col.r = col.g = col.b = l;
    }
    else
    {
        float v1, v2;
        float hue = (float)h / 360.0f;

        v2 = (l < 0.5) ? (l * (1 + s)) : ((l + s) - (l * s));
        v1 = 2 * l - v2;

        col.r = HueToRGB(v1, v2, hue + (1.0f / 3));
        col.g = HueToRGB(v1, v2, hue);
        col.b = HueToRGB(v1, v2, hue - (1.0f / 3));
    }

    return col;
}

ColorRGBA ColorRGBA::FromHSLA(float h, float s, float l, float a) {
    auto col = FromHSL(h,s,l);
    col.a = a;
    return col;
}

ColorRGBA ColorRGBA::FromHexStr(std::string &str) {
    ColorRGBA rgba;
    return rgba;
}

ColorRGBA ColorRGBA::FromRGB(uint8_t red, uint8_t green, uint8_t blue) {
    ColorRGBA col;
    col.r = red / 255.0f;
    col.g = green / 255.0f;
    col.b = blue / 255.0f;
    return col;
}
ColorRGBA ColorRGBA::FromRGB(uint32_t red, uint32_t green, uint32_t blue) {
    ColorRGBA col;
    col.r = red / 255.0f;
    col.g = green / 255.0f;
    col.b = blue / 255.0f;
    return col;
}
ColorRGBA ColorRGBA::FromRGB(int red, int green, int blue) {
    ColorRGBA col;
    col.r = red / 255.0f;
    col.g = green / 255.0f;
    col.b = blue / 255.0f;
    return col;
}


ColorRGBA ColorRGBA::FromRGB(float r, float g, float b) {
    ColorRGBA col;
    col.r = r;
    col.g = g;
    col.b = b;
    return col;
}
ColorRGBA ColorRGBA::FromRGBA(float r, float g, float b, float a) {
    ColorRGBA col;
    col.r = r;
    col.g = g;
    col.b = b;
    col.a = a;
    return col;
}

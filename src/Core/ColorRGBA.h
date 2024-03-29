//
// Created by gnilk on 25.01.23.
//

#ifndef EDITOR_COLORRGBA_H
#define EDITOR_COLORRGBA_H

#include <cstdint>
#include <string>

// TO-DO:
//  - clamping and satmul/satadd/etc...
//  - Hash value function

class ColorRGBA {
public:
    //constexpr ColorRGBA() : r(1.0f), g(1.0f), b(1.0f), a(0.0f) { } ;
    ColorRGBA() = default;

    static ColorRGBA FromRGB(float r, float g, float b);
    static ColorRGBA FromRGBA(float r, float g, float b, float a);
    static ColorRGBA FromRGBA(int r, int g, int b, int a);
    static ColorRGBA FromRGB(uint8_t r, uint8_t g, uint8_t b);
    static ColorRGBA FromRGB(int r, int g, int b);
    static ColorRGBA FromRGB(uint32_t r, uint32_t g, uint32_t b);
    static ColorRGBA FromHSL(float h, float s, float l);
    static ColorRGBA FromHSLA(float h, float s, float l, float a);
    static ColorRGBA FromHexStr(std::string &str);

    bool operator == (const ColorRGBA &other) const {
        if (other.RedAsInt() != RedAsInt()) return false;
        if (other.GreenAsInt() != GreenAsInt()) return false;
        if (other.BlueAsInt() != BlueAsInt()) return false;
        if (other.AlphaAsInt() != AlphaAsInt()) return false;
        return true;
    }

    size_t Hash() const {
        return ((RedAsInt() << 24) | (GreenAsInt() << 16) | (BlueAsInt() << 8) | (AlphaAsInt()));
    }

    const ColorRGBA operator *(float v) const {
        ColorRGBA dst;
        dst.r = r * v;
        dst.g = g * v;
        dst.b = b * v;
        dst.a = a * v;
        return dst;
    }

    const ColorRGBA operator *(const ColorRGBA &v) const {
        ColorRGBA dst;
        dst.r = r * v.r;
        dst.g = g * v.g;
        dst.b = b * v.b;
        dst.a = a * v.a;
        return dst;
    }

    void SetAlpha(float alpha) {
        a = alpha;
    }

    int RedAsInt(int mul = 255) const {
        return r * (float)mul;
    }
    int GreenAsInt(int mul = 255) const {
        return g * (float)mul;
    }
    int BlueAsInt(int mul = 255) const {
        return b * (float)mul;
    }
    int AlphaAsInt(int mul = 255) const {
        return a * (float)mul;
    }

    // Default is 'Float' and we need these shortcuts as the current JS integration can't handle default args
    int IntRed() const {
        return RedAsInt();
    }
    int IntGreen() const {
        return GreenAsInt();
    }
    int IntBlue() const {
        return BlueAsInt();
    }
    int IntAlpha() const {
        return AlphaAsInt();
    }

    float R() const { return r; }
    float G() const { return g; }
    float B() const { return b; }
    float A() const { return a;}

private:
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

#endif //EDITOR_COLORRGBA_H

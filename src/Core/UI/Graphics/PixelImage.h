//
// PixelImage - pixels for DrawContext::DrawImage (docs/usability-round.md §7): RGBA bytes, row after row, top first.
// Plain data - whoever builds it knows nothing of the backend, the backend nothing of what it shows. 'revision' changes
// when the pixels do, so a backend keeping them in a texture uploads them only then.
//
#ifndef GEDIT_PIXELIMAGE_H
#define GEDIT_PIXELIMAGE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "Core/ColorRGBA.h"

namespace gedit {

    class PixelImage {
    public:
        using Ref = std::shared_ptr<PixelImage>;

        static constexpr int kBytesPerPixel = 4;

    public:
        PixelImage() = default;
        virtual ~PixelImage() = default;

        static Ref Create() {
            return std::make_shared<PixelImage>();
        }

        // A new size, every pixel 'colour' - and a new revision
        void Reset(int newWidth, int newHeight, const ColorRGBA &colour) {
            width = (newWidth > 0) ? newWidth : 0;
            height = (newHeight > 0) ? newHeight : 0;
            rgba.resize((size_t)width * (size_t)height * kBytesPerPixel);
            for (size_t i = 0; i < rgba.size(); i += kBytesPerPixel) {
                rgba[i + 0] = (uint8_t)colour.RedAsInt();
                rgba[i + 1] = (uint8_t)colour.GreenAsInt();
                rgba[i + 2] = (uint8_t)colour.BlueAsInt();
                rgba[i + 3] = (uint8_t)colour.AlphaAsInt();
            }
            revision++;
        }

        void SetPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
            if ((x < 0) || (y < 0) || (x >= width) || (y >= height)) {
                return;
            }
            auto idx = ((size_t)y * (size_t)width + (size_t)x) * kBytesPerPixel;
            rgba[idx + 0] = r;
            rgba[idx + 1] = g;
            rgba[idx + 2] = b;
            rgba[idx + 3] = a;
        }

        // 'other' with its top left at (x, y), cut at this image's edges
        void CopyFrom(const PixelImage &other, int x, int y) {
            for (int row = 0; row < other.height; row++) {
                auto dstY = y + row;
                if ((dstY < 0) || (dstY >= height)) {
                    continue;
                }
                auto first = std::max(0, -x);
                auto last = std::min(other.width, width - x);
                if (first >= last) {
                    return;
                }
                auto src = other.rgba.data() + ((size_t)row * other.width + first) * kBytesPerPixel;
                auto dst = rgba.data() + ((size_t)dstY * width + (x + first)) * kBytesPerPixel;
                std::memcpy(dst, src, (size_t)(last - first) * kBytesPerPixel);
            }
        }

        // A rectangle of 'colour', its alpha laid over what is there (1: replaces it)
        void BlendRect(int x, int y, int w, int h, const ColorRGBA &colour) {
            auto x0 = std::max(x, 0);
            auto y0 = std::max(y, 0);
            auto x1 = std::min(x + w, width);
            auto y1 = std::min(y + h, height);
            auto alpha = (float)colour.AlphaAsInt() / 255.0f;
            auto r = (float)colour.RedAsInt();
            auto g = (float)colour.GreenAsInt();
            auto b = (float)colour.BlueAsInt();
            for (int row = y0; row < y1; row++) {
                for (int col = x0; col < x1; col++) {
                    auto idx = ((size_t)row * width + col) * kBytesPerPixel;
                    rgba[idx + 0] = (uint8_t)((float)rgba[idx + 0] + (r - (float)rgba[idx + 0]) * alpha);
                    rgba[idx + 1] = (uint8_t)((float)rgba[idx + 1] + (g - (float)rgba[idx + 1]) * alpha);
                    rgba[idx + 2] = (uint8_t)((float)rgba[idx + 2] + (b - (float)rgba[idx + 2]) * alpha);
                }
            }
        }

        // The pixel as 0xRRGGBBAA; 0 outside the image - for the tests
        uint32_t PixelAt(int x, int y) const {
            if ((x < 0) || (y < 0) || (x >= width) || (y >= height)) {
                return 0;
            }
            auto idx = ((size_t)y * (size_t)width + (size_t)x) * kBytesPerPixel;
            return ((uint32_t)rgba[idx] << 24) | ((uint32_t)rgba[idx + 1] << 16) | ((uint32_t)rgba[idx + 2] << 8) |
                   (uint32_t)rgba[idx + 3];
        }

        int Width() const {
            return width;
        }
        int Height() const {
            return height;
        }
        bool IsEmpty() const {
            return (width == 0) || (height == 0);
        }
        const uint8_t *Data() const {
            return rgba.data();
        }
        int Pitch() const {
            return width * kBytesPerPixel;
        }
        uint64_t Revision() const {
            return revision;
        }
        // The pixels were changed with SetPixel - the backend uploads them again
        void Touch() {
            revision++;
        }

    private:
        int width = 0;
        int height = 0;
        std::vector<uint8_t> rgba;
        uint64_t revision = 0;
    };

}

#endif //GEDIT_PIXELIMAGE_H

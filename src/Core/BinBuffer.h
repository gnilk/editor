//
// Created by gnilk on 10.06.26.
//
// BinBuffer — a flat, owned byte container (conceptually a `uint8_t* + size_t`). The end-game value
// type for "a file as bytes" (hex view, and later raw/binary buffers). Deliberately dumb: it carries
// bytes and nothing else. Producing the bytes (e.g. UTF-32 -> UTF-8 of a TextBuffer) is a ByteStreamReader's
// job; interpreting them as coordinates is HexProjection's job. Backed by std::vector for now; the
// public surface is small enough to swap to a raw pointer + length later without touching callers.
//

#ifndef EDITOR_BINBUFFER_H
#define EDITOR_BINBUFFER_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace gedit {

    class BinBuffer {
    public:
        using Ref = std::shared_ptr<BinBuffer>;

        BinBuffer() = default;

        static Ref Create() {
            return std::make_shared<BinBuffer>();
        }
        static Ref CreateFrom(const uint8_t *src, size_t numBytes) {
            auto buffer = Create();
            buffer->Append(src, numBytes);
            return buffer;
        }

        void Reserve(size_t numBytes) { bytes.reserve(numBytes); }
        void Clear() { bytes.clear(); }

        void Append(uint8_t byte) {
            size_t at = bytes.size();
            bytes.resize(at + 1);
            bytes[at] = byte;
        }
        void Append(const uint8_t *src, size_t numBytes) {
            if ((src != nullptr) && (numBytes > 0)) {
                size_t at = bytes.size();
                bytes.resize(at + numBytes);
                memcpy(bytes.data() + at, src, numBytes);
            }
        }

        const uint8_t *Data() const { return bytes.data(); }
        size_t Size() const { return bytes.size(); }
        bool Empty() const { return bytes.empty(); }
        uint8_t At(size_t idx) const { return bytes[idx]; }

        // Copy up to maxBytes starting at 'offset' into dst; returns the number actually copied
        // (fewer than maxBytes only when the offset+maxBytes window runs past the end).
        size_t ReadBytes(size_t offset, void *dst, size_t maxBytes) const {
            if ((dst == nullptr) || (offset >= bytes.size())) {
                return 0;
            }
            size_t avail = bytes.size() - offset;
            size_t n = (maxBytes < avail) ? maxBytes : avail;
            memcpy(dst, bytes.data() + offset, n);
            return n;
        }

    private:
        std::vector<uint8_t> bytes;
    };
}

#endif //EDITOR_BINBUFFER_H

//
// Created by gnilk on 10.06.26.
//
// HexProjection — pure, stateless coordinate translation between text space (line + char-index, the
// canonical model coordinate) and the flat UTF-8 byte stream a hex view addresses. It does NOT own or
// produce the bytes: the caller hands in the already-converted BinBuffer (from a ByteStreamReader).
// Keeping it conversion-free is the whole point - the model stays UTF-32/text, the byte stream is an
// argument, and this just does the index arithmetic between the two.
//
// The BinBuffer alone is self-describing for this: 0x0A is the line separator (it never appears inside
// a multibyte UTF-8 sequence), and char boundaries are the non-continuation bytes ((b & 0xC0) != 0x80).
// So no TextBuffer is needed here - the byte stream is the single source of truth for the mapping.
//
// These two are the user's "ProjectUTF32ToUTF8" / "ProjectUTF8ToUTF32", named by what they carry:
// a text Point (x=charIdx, y=lineIdx) <-> an absolute byte offset.
//

#ifndef EDITOR_HEXPROJECTION_H
#define EDITOR_HEXPROJECTION_H

#include <cstddef>

#include "Core/BinBuffer.h"
#include "Core/Point.h"

namespace gedit {

    class HexProjection {
    public:
        // Text position (x=charIdx, y=lineIdx) -> absolute byte offset in the UTF-8 stream.
        // The line and char indices are clamped to what the stream actually contains.
        static size_t TextToByteOffset(const Point &textPos, const BinBuffer &utf8);

        // Absolute byte offset -> text position. The offset is clamped to [0, Size()]; an offset
        // landing mid-multibyte-char or on a line separator snaps to the nearest char boundary at or
        // before it.
        static Point ByteOffsetToText(size_t byteOffset, const BinBuffer &utf8);

        // First byte index of the char that starts after 'pos' (skips UTF-8 continuation bytes). Public
        // because hex navigation needs to step the caret to the next char boundary (a one-byte step
        // right that lands mid-multibyte char would otherwise snap back to the same char).
        static size_t NextCharStart(const BinBuffer &utf8, size_t pos);
    };
}

#endif //EDITOR_HEXPROJECTION_H

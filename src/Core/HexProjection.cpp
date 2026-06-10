//
// Created by gnilk on 10.06.26.
//

#include "Core/HexProjection.h"

using namespace gedit;

size_t HexProjection::NextCharStart(const BinBuffer &utf8, size_t pos) {
    size_t size = utf8.Size();
    size_t q = pos + 1;
    while ((q < size) && ((utf8.At(q) & 0xC0) == 0x80)) {
        ++q;
    }
    return q;
}

size_t HexProjection::TextToByteOffset(const Point &textPos, const BinBuffer &utf8) {
    size_t size = utf8.Size();
    int targetLine = (textPos.y < 0) ? 0 : textPos.y;
    int targetChar = (textPos.x < 0) ? 0 : textPos.x;

    // Walk to the start of the target line (just past the targetLine-th 0x0A), tracking the most
    // recent line start so an out-of-range line index clamps to the LAST line's start.
    size_t offset = 0;
    size_t lineStartOffset = 0;
    int linesSeen = 0;
    while ((offset < size) && (linesSeen < targetLine)) {
        if (utf8.At(offset) == 0x0A) {
            ++linesSeen;
            lineStartOffset = offset + 1;
        }
        ++offset;
    }
    if (linesSeen < targetLine) {
        // Ran past the end before reaching targetLine - clamp to the final line's start.
        offset = lineStartOffset;
    }

    // Advance targetChar chars within the line, stopping at the separator / end.
    int charsSeen = 0;
    while ((charsSeen < targetChar) && (offset < size) && (utf8.At(offset) != 0x0A)) {
        offset = NextCharStart(utf8, offset);
        ++charsSeen;
    }
    return offset;
}

Point HexProjection::ByteOffsetToText(size_t byteOffset, const BinBuffer &utf8) {
    size_t size = utf8.Size();
    if (byteOffset > size) {
        byteOffset = size;
    }

    // Line = number of separators before the offset; lineStart = byte just past the last one.
    int line = 0;
    size_t lineStart = 0;
    for (size_t i = 0; i < byteOffset; ++i) {
        if (utf8.At(i) == 0x0A) {
            ++line;
            lineStart = i + 1;
        }
    }

    // Char index = count of full chars whose start is < byteOffset; an offset inside a char snaps down.
    int charIdx = 0;
    size_t p = lineStart;
    while (p < byteOffset) {
        size_t next = NextCharStart(utf8, p);
        if (next > byteOffset) {
            break;
        }
        p = next;
        ++charIdx;
    }
    return {charIdx, line};
}

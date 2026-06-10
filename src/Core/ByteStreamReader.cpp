//
// Created by gnilk on 10.06.26.
//

#include "Core/ByteStreamReader.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

ByteStreamReader::Ref ByteStreamReader::Create(TextBuffer::Ref buffer) {
    // CTOR is protected - can't make_shared directly.
    auto ptr = new ByteStreamReader(buffer);
    return std::shared_ptr<ByteStreamReader>(ptr);
}

ByteStreamReader::ByteStreamReader(TextBuffer::Ref buffer) : textBuffer(buffer) {
    binBuffer = BinBuffer::Create();
    Rebuild();
}

void ByteStreamReader::Rebuild() {
    binBuffer->Clear();

    size_t numLines = textBuffer->NumLines();
    for (size_t idxLine = 0; idxLine < numLines; ++idxLine) {
        // 0x0A separates lines; it does NOT terminate the final line.
        if (idxLine > 0) {
            binBuffer->Append(static_cast<uint8_t>(0x0A));
        }
        std::string utf8 = UnicodeHelper::utf32to8(textBuffer->LineAt(idxLine)->Buffer());
        binBuffer->Append(reinterpret_cast<const uint8_t *>(utf8.data()), utf8.size());
    }
}

size_t ByteStreamReader::ReadBytes(size_t offset, void *dst, size_t maxBytes) const {
    return binBuffer->ReadBytes(offset, dst, maxBytes);
}

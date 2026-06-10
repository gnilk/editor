//
// Created by gnilk on 10.06.26.
//
// ByteStreamReader — turns a TextBuffer (UTF-32 lines) into a flat UTF-8 byte stream and serves reads
// out of it. This is the ONE place the UTF-32 -> UTF-8 conversion lives; HexProjection (coordinate
// math) and the hex view stay conversion-free.
//
// Byte-stream definition: for each line, UTF-8(line content) followed by a single 0x0A separator
// BETWEEN lines only - the last line contributes just its own bytes (no synthetic trailing newline;
// a real trailing newline is already the empty final line in the TextBuffer).
//
// v1 materializes the whole stream into a BinBuffer up front (Create). The read interface
// (ReadBytes(offset, ...)) is intentionally offset-addressed and decoupled from that, so a future
// streaming/windowed converter (encode-on-demand, no full materialization) can replace the eager
// build without touching callers.
//

#ifndef EDITOR_BYTESTREAMREADER_H
#define EDITOR_BYTESTREAMREADER_H

#include <cstddef>
#include <memory>

#include "Core/BinBuffer.h"
#include "Core/TextBuffer.h"

namespace gedit {

    class ByteStreamReader {
    public:
        using Ref = std::shared_ptr<ByteStreamReader>;

        static Ref Create(TextBuffer::Ref buffer);
        virtual ~ByteStreamReader() = default;

        // Re-materialize the byte stream from the (possibly changed) text buffer.
        void Rebuild();

        size_t Size() const { return binBuffer->Size(); }

        // Copy up to maxBytes from byte 'offset' into dst; returns the number actually read.
        size_t ReadBytes(size_t offset, void *dst, size_t maxBytes) const;

        // The materialized UTF-8 bytes - hand this to HexProjection for coordinate translation.
        BinBuffer::Ref GetBuffer() const { return binBuffer; }

    protected:
        explicit ByteStreamReader(TextBuffer::Ref buffer);

    private:
        TextBuffer::Ref textBuffer;
        BinBuffer::Ref binBuffer;
    };
}

#endif //EDITOR_BYTESTREAMREADER_H

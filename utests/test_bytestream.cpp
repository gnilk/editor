//
// Created by gnilk on 10.06.26.
//
// ByteStreamReader (+ BinBuffer): the UTF-32 -> UTF-8 conversion and byte-read interface that feeds
// the hex view. Verifies the byte-stream definition (UTF-8 content, 0x0A between lines, no synthetic
// trailing newline), offset-addressed reads (full + every window), and that HexProjection round-trips
// over the produced bytes for real multibyte content.
//
#include <testinterface.h>

#include <string>
#include <vector>

#include "Core/BinBuffer.h"
#include "Core/ByteStreamReader.h"
#include "Core/HexProjection.h"
#include "Core/TextBuffer.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_bytestream(ITesting *t);
DLL_EXPORT int test_bytestream_binbuffer(ITesting *t);
DLL_EXPORT int test_bytestream_convert(ITesting *t);
DLL_EXPORT int test_bytestream_lastline(ITesting *t);
DLL_EXPORT int test_bytestream_windows(ITesting *t);
DLL_EXPORT int test_bytestream_projection_roundtrip(ITesting *t);
}

static TextBuffer::Ref MakeBuffer(const std::vector<std::u32string> &lines) {
    auto buffer = TextBuffer::CreateEmptyBuffer();
    buffer->DeleteLineAt(0);
    for (auto &line : lines) {
        buffer->AddLine(line);
    }
    return buffer;
}

// UTF-8(line) joined by 0x0A, no trailing separator.
static std::string ExpectedStream(const std::vector<std::u32string> &lines) {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            out.push_back('\n');
        }
        out += UnicodeHelper::utf32to8(lines[i]);
    }
    return out;
}

DLL_EXPORT int test_bytestream(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_bytestream_binbuffer(ITesting *t) {
    auto bin = BinBuffer::CreateFrom(reinterpret_cast<const uint8_t *>("hello"), 5);
    TR_ASSERT(t, bin->Size() == 5);
    TR_ASSERT(t, bin->At(0) == 'h');

    uint8_t out[8] = {};
    TR_ASSERT(t, bin->ReadBytes(0, out, 5) == 5);
    TR_ASSERT(t, std::string(reinterpret_cast<char *>(out), 5) == "hello");
    // Windowed read.
    TR_ASSERT(t, bin->ReadBytes(2, out, 2) == 2);
    TR_ASSERT(t, std::string(reinterpret_cast<char *>(out), 2) == "ll");
    // Past-end / clamped reads.
    TR_ASSERT(t, bin->ReadBytes(3, out, 99) == 2);
    TR_ASSERT(t, bin->ReadBytes(5, out, 4) == 0);
    TR_ASSERT(t, bin->ReadBytes(100, out, 4) == 0);
    return kTR_Pass;
}

DLL_EXPORT int test_bytestream_convert(ITesting *t) {
    std::vector<std::u32string> lines = {U"abc", U"de"};
    auto reader = ByteStreamReader::Create(MakeBuffer(lines));

    std::string expected = ExpectedStream(lines);
    TR_ASSERT(t, reader->Size() == expected.size());
    TR_ASSERT(t, reader->Size() == 6);

    auto bin = reader->GetBuffer();
    TR_ASSERT(t, bin != nullptr);
    std::string got(reinterpret_cast<const char *>(bin->Data()), bin->Size());
    TR_ASSERT(t, got == expected);
    TR_ASSERT(t, got == std::string("abc\nde"));
    return kTR_Pass;
}

DLL_EXPORT int test_bytestream_lastline(ITesting *t) {
    // ["abc"] -> 3 bytes; ["abc",""] -> 4 bytes (trailing newline == empty final line).
    auto noTrailing = ByteStreamReader::Create(MakeBuffer({U"abc"}));
    TR_ASSERT(t, noTrailing->Size() == 3);

    auto withTrailing = ByteStreamReader::Create(MakeBuffer({U"abc", U""}));
    TR_ASSERT(t, withTrailing->Size() == 4);
    auto bin = withTrailing->GetBuffer();
    TR_ASSERT(t, bin->At(3) == 0x0A);
    return kTR_Pass;
}

DLL_EXPORT int test_bytestream_windows(ITesting *t) {
    // Multibyte content; every (offset,length) read window must match the expected slice - exercises
    // the read interface across the multibyte and separator bytes.
    std::vector<std::u32string> lines = {U"hello", U"w€rld", U"", U"end"};
    auto reader = ByteStreamReader::Create(MakeBuffer(lines));
    std::string expected = ExpectedStream(lines);
    TR_ASSERT(t, reader->Size() == expected.size());

    for (size_t off = 0; off < expected.size(); ++off) {
        for (size_t len = 1; off + len <= expected.size(); ++len) {
            uint8_t win[64] = {};
            size_t got = reader->ReadBytes(off, win, len);
            TR_ASSERT(t, got == len);
            TR_ASSERT(t, std::string(reinterpret_cast<char *>(win), got) == expected.substr(off, len));
        }
    }
    return kTR_Pass;
}

DLL_EXPORT int test_bytestream_projection_roundtrip(ITesting *t) {
    // The reader's bytes + HexProjection together round-trip every text position - the end-to-end
    // contract a real document exercises (multibyte chars, empty + trailing-empty lines).
    std::vector<std::u32string> lines = {U"a€b", U"", U"xé", U""};
    auto reader = ByteStreamReader::Create(MakeBuffer(lines));
    auto bin = reader->GetBuffer();

    for (int y = 0; y < (int)lines.size(); ++y) {
        int len = (int)lines[y].length();
        for (int x = 0; x <= len; ++x) {
            Point in(x, y);
            size_t off = HexProjection::TextToByteOffset(in, *bin);
            Point out = HexProjection::ByteOffsetToText(off, *bin);
            TR_ASSERT(t, out.x == in.x);
            TR_ASSERT(t, out.y == in.y);
        }
    }
    return kTR_Pass;
}

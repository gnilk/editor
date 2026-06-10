//
// Created by gnilk on 10.06.26.
//
// H.1 of the HexView spike (docs/workspace-refactor-plan.md): the coordinate translation between text
// space (line + char-index) and the flat UTF-8 byte stream. HexProjection is PURE - it takes a
// BinBuffer of bytes and does index math, no TextBuffer and no conversion. These cases therefore feed
// hand-built byte buffers directly (which also proves the decoupling): multibyte chars, line
// boundaries, first/last line, clamps, the trailing-newline edge, and full round-trip identity.
//
#include <testinterface.h>

#include <string>
#include <vector>

#include "Core/BinBuffer.h"
#include "Core/HexProjection.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_hexprojection(ITesting *t);
DLL_EXPORT int test_hexprojection_empty(ITesting *t);
DLL_EXPORT int test_hexprojection_ascii(ITesting *t);
DLL_EXPORT int test_hexprojection_multibyte(ITesting *t);
DLL_EXPORT int test_hexprojection_lastline(ITesting *t);
DLL_EXPORT int test_hexprojection_clamp(ITesting *t);
DLL_EXPORT int test_hexprojection_roundtrip(ITesting *t);
}

// Build a BinBuffer straight from a raw byte string (no TextBuffer involved).
static BinBuffer::Ref Bytes(const std::string &raw) {
    return BinBuffer::CreateFrom(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
}

DLL_EXPORT int test_hexprojection(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_hexprojection_empty(ITesting *t) {
    auto bin = Bytes("");
    TR_ASSERT(t, bin->Size() == 0);
    TR_ASSERT(t, HexProjection::TextToByteOffset({0, 0}, *bin) == 0);

    auto p = HexProjection::ByteOffsetToText(0, *bin);
    TR_ASSERT(t, p.x == 0);
    TR_ASSERT(t, p.y == 0);
    return kTR_Pass;
}

DLL_EXPORT int test_hexprojection_ascii(ITesting *t) {
    // "abc" \n "de"
    auto bin = Bytes("abc\nde");
    TR_ASSERT(t, bin->Size() == 6);

    TR_ASSERT(t, HexProjection::TextToByteOffset({0, 0}, *bin) == 0);
    TR_ASSERT(t, HexProjection::TextToByteOffset({3, 0}, *bin) == 3);   // EOL of line 0 (separator slot)
    TR_ASSERT(t, HexProjection::TextToByteOffset({0, 1}, *bin) == 4);   // start of line 1
    TR_ASSERT(t, HexProjection::TextToByteOffset({2, 1}, *bin) == 6);   // EOL of line 1 == Size

    // The separator byte (offset 3) snaps back to EOL of line 0.
    auto sep = HexProjection::ByteOffsetToText(3, *bin);
    TR_ASSERT(t, sep.x == 3);
    TR_ASSERT(t, sep.y == 0);
    // First byte of line 1.
    auto l1 = HexProjection::ByteOffsetToText(4, *bin);
    TR_ASSERT(t, l1.x == 0);
    TR_ASSERT(t, l1.y == 1);
    return kTR_Pass;
}

DLL_EXPORT int test_hexprojection_multibyte(ITesting *t) {
    // 'a'(1) + U+20AC euro(0xE2 0x82 0xAC) + 'b'(1) = 5 content bytes, single line.
    auto bin = Bytes("a\xE2\x82\xAC" "b");
    TR_ASSERT(t, bin->Size() == 5);

    // Char-index -> byte offset accounts for the 3-byte euro.
    TR_ASSERT(t, HexProjection::TextToByteOffset({0, 0}, *bin) == 0);  // 'a'
    TR_ASSERT(t, HexProjection::TextToByteOffset({1, 0}, *bin) == 1);  // euro start
    TR_ASSERT(t, HexProjection::TextToByteOffset({2, 0}, *bin) == 4);  // 'b' start (1 + 3)
    TR_ASSERT(t, HexProjection::TextToByteOffset({3, 0}, *bin) == 5);  // EOL

    // A byte offset landing mid-euro snaps down to the euro's char start (index 1).
    TR_ASSERT(t, HexProjection::ByteOffsetToText(1, *bin).x == 1);
    TR_ASSERT(t, HexProjection::ByteOffsetToText(2, *bin).x == 1);
    TR_ASSERT(t, HexProjection::ByteOffsetToText(3, *bin).x == 1);
    TR_ASSERT(t, HexProjection::ByteOffsetToText(4, *bin).x == 2);
    return kTR_Pass;
}

DLL_EXPORT int test_hexprojection_lastline(ITesting *t) {
    // THE edge case: a trailing on-disk newline is the empty final line.
    //   "abc"    -> 3 bytes, no trailing 0x0A
    //   "abc\n"  -> 4 bytes (the empty last line yields the trailing newline)
    auto noTrailing = Bytes("abc");
    TR_ASSERT(t, HexProjection::TextToByteOffset({3, 0}, *noTrailing) == 3);

    auto withTrailing = Bytes("abc\n");
    // Offset at the empty last line == Size; round-trips to (0, lastLine).
    auto p = HexProjection::ByteOffsetToText(4, *withTrailing);
    TR_ASSERT(t, p.y == 1);
    TR_ASSERT(t, p.x == 0);
    TR_ASSERT(t, HexProjection::TextToByteOffset({0, 1}, *withTrailing) == 4);
    return kTR_Pass;
}

DLL_EXPORT int test_hexprojection_clamp(ITesting *t) {
    auto bin = Bytes("abc\nde");

    // Negative / past-end line index clamps into range.
    TR_ASSERT(t, HexProjection::TextToByteOffset({0, -5}, *bin) == 0);
    TR_ASSERT(t, HexProjection::TextToByteOffset({0, 99}, *bin) == 4);   // clamps to last line start
    // Char index past EOL clamps to EOL (stops at separator / end).
    TR_ASSERT(t, HexProjection::TextToByteOffset({99, 0}, *bin) == 3);
    TR_ASSERT(t, HexProjection::TextToByteOffset({-3, 0}, *bin) == 0);

    // Byte offset past the end clamps to Size.
    auto p = HexProjection::ByteOffsetToText(9999, *bin);
    TR_ASSERT(t, p.x == 2);
    TR_ASSERT(t, p.y == 1);
    return kTR_Pass;
}

DLL_EXPORT int test_hexprojection_roundtrip(ITesting *t) {
    // Every valid (line, charIdx) position survives Point -> byteOffset -> Point unchanged.
    // Stream: "a€b" \n "" \n "xé" \n ""  (multibyte chars, empty line, trailing empty line).
    auto bin = Bytes("a\xE2\x82\xAC" "b\n\nx\xC3\xA9\n");

    // (lineLength in CHARS for each line above)
    std::vector<int> lineLens = {3, 0, 2, 0};
    for (int y = 0; y < (int)lineLens.size(); ++y) {
        for (int x = 0; x <= lineLens[y]; ++x) {
            Point in(x, y);
            size_t off = HexProjection::TextToByteOffset(in, *bin);
            Point out = HexProjection::ByteOffsetToText(off, *bin);
            TR_ASSERT(t, out.x == in.x);
            TR_ASSERT(t, out.y == in.y);
        }
    }
    return kTR_Pass;
}
